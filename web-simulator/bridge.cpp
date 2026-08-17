// SPDX-License-Identifier: MIT

#include "device/rtl_sdr_device.h"
#include "dsp/iq_spectrum.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

using namespace std::chrono_literals;

constexpr uint32_t kMinimumFrequencyHz = 22'000'000;
constexpr uint32_t kMaximumFrequencyHz = 948'600'000;
constexpr uint32_t kDefaultFrequencyHz = 97'400'000;
constexpr uint32_t kSampleRateHz = 2'048'000;
constexpr size_t kIqBlockBytes = 16 * 16'384;
constexpr uint16_t kDefaultPort = 18'117;

std::atomic_bool running{true};
std::mutex control_rate_mutex;
std::chrono::steady_clock::time_point control_window_started{
    std::chrono::steady_clock::now()};
unsigned control_requests_in_window{0};

void handle_signal(int) {
    running.store(false);
}

std::string json_escape(std::string_view input) {
    std::ostringstream out;
    for (const unsigned char ch : input) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(ch) << std::dec;
                } else {
                    out << static_cast<char>(ch);
                }
        }
    }
    return out.str();
}

struct BridgeState {
    mutable std::mutex mutex;
    std::string status{"connecting"};
    std::string detail{"starting RTL-SDR bridge"};
    std::string device_name;
    uint32_t frequency_hz{kDefaultFrequencyHz};
    bool automatic_gain{true};
    int gain_tenths_db{192};
    std::vector<int> supported_gains;
    std::array<uint8_t, dsp::kSpectrumBinCount> spectrum{};
    uint64_t frame_sequence{0};
    uint64_t iq_blocks{0};
    uint64_t iq_bytes{0};
    uint64_t read_errors{0};
    uint64_t reconnects{0};
    std::chrono::steady_clock::time_point started_at{std::chrono::steady_clock::now()};
};

struct RequestedControl {
    std::atomic<uint32_t> frequency_hz{kDefaultFrequencyHz};
    std::atomic<bool> automatic_gain{true};
    std::atomic<int> gain_tenths_db{192};
    std::atomic<uint64_t> revision{0};
};

BridgeState bridge_state;
RequestedControl requested_control;

void publish_status(std::string status, std::string detail = {}, std::string device_name = {}) {
    std::lock_guard lock(bridge_state.mutex);
    bridge_state.status = std::move(status);
    bridge_state.detail = std::move(detail);
    if (!device_name.empty()) bridge_state.device_name = std::move(device_name);
}

void interruptible_pause(std::chrono::milliseconds duration) {
    const auto deadline = std::chrono::steady_clock::now() + duration;
    while (running.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(100ms);
    }
}

void receiver_loop() {
    while (running.load()) {
        device::RtlSdrDevice radio;
        if (!radio.library_available()) {
            publish_status("library_missing", radio.library_error());
            interruptible_pause(2s);
            continue;
        }

        const auto devices = radio.devices();
        if (devices.empty()) {
            publish_status("missing", "no supported RTL-SDR device detected");
            interruptible_pause(2s);
            continue;
        }

        std::string error;
        const auto open_result = radio.open(devices.front().index, error);
        if (open_result != device::RtlSdrOpenResult::Opened) {
            const char* status = "error";
            if (open_result == device::RtlSdrOpenResult::AccessDenied) status = "access";
            if (open_result == device::RtlSdrOpenResult::Busy) status = "busy";
            if (open_result == device::RtlSdrOpenResult::Disconnected) status = "missing";
            publish_status(status, error, devices.front().name);
            interruptible_pause(2s);
            continue;
        }

        const auto gains = radio.tuner_gains(error);
        const uint32_t initial_frequency = requested_control.frequency_hz.load();
        const bool initial_auto_gain = requested_control.automatic_gain.load();
        const int initial_gain = requested_control.gain_tenths_db.load();
        if (gains.empty() || !radio.configure(initial_frequency,
                                               kSampleRateHz,
                                               initial_auto_gain,
                                               initial_gain,
                                               error)) {
            publish_status("error", error, devices.front().name);
            radio.close();
            interruptible_pause(2s);
            continue;
        }

        {
            std::lock_guard lock(bridge_state.mutex);
            bridge_state.status = "live";
            bridge_state.detail.clear();
            bridge_state.device_name = devices.front().name;
            bridge_state.frequency_hz = initial_frequency;
            bridge_state.automatic_gain = initial_auto_gain;
            bridge_state.gain_tenths_db = initial_gain;
            bridge_state.supported_gains = gains;
            ++bridge_state.reconnects;
        }

        dsp::IqSpectrum spectrum_processor;
        std::vector<uint8_t> iq(kIqBlockBytes);
        uint64_t applied_revision = requested_control.revision.load();

        while (running.load() && radio.is_open()) {
            const uint64_t revision = requested_control.revision.load();
            if (revision != applied_revision) {
                const uint32_t frequency = requested_control.frequency_hz.load();
                const bool automatic_gain = requested_control.automatic_gain.load();
                const int gain = requested_control.gain_tenths_db.load();
                if (!radio.set_center_frequency(frequency, error) ||
                    !radio.set_gain(automatic_gain, gain, error) ||
                    !radio.reset_buffer(error)) {
                    publish_status("error", error, devices.front().name);
                    break;
                }
                {
                    std::lock_guard lock(bridge_state.mutex);
                    bridge_state.frequency_hz = frequency;
                    bridge_state.automatic_gain = automatic_gain;
                    bridge_state.gain_tenths_db = gain;
                }
                applied_revision = revision;
            }

            size_t bytes_read = 0;
            if (!radio.read_sync(iq.data(), iq.size(), bytes_read, error) || bytes_read == 0) {
                {
                    std::lock_guard lock(bridge_state.mutex);
                    ++bridge_state.read_errors;
                }
                publish_status("error", error.empty() ? "RTL-SDR returned no IQ data" : error,
                               devices.front().name);
                break;
            }

            const auto frame = spectrum_processor.process(iq.data(), bytes_read);
            {
                std::lock_guard lock(bridge_state.mutex);
                bridge_state.status = "live";
                bridge_state.detail.clear();
                bridge_state.spectrum = frame.level;
                bridge_state.frame_sequence = frame.sequence;
                ++bridge_state.iq_blocks;
                bridge_state.iq_bytes += bytes_read;
            }
        }

        radio.close();
        if (running.load()) interruptible_pause(1s);
    }
}

std::string status_json() {
    std::lock_guard lock(bridge_state.mutex);
    const auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - bridge_state.started_at).count();
    std::ostringstream out;
    out << "{\"schema\":\"zero-sdr-bridge-v1\",\"status\":\""
        << json_escape(bridge_state.status) << "\",\"detail\":\""
        << json_escape(bridge_state.detail) << "\",\"device_name\":\""
        << json_escape(bridge_state.device_name) << "\",\"frequency_hz\":"
        << bridge_state.frequency_hz << ",\"sample_rate_hz\":" << kSampleRateHz
        << ",\"automatic_gain\":" << (bridge_state.automatic_gain ? "true" : "false")
        << ",\"gain_tenths_db\":" << bridge_state.gain_tenths_db
        << ",\"frame_sequence\":" << bridge_state.frame_sequence
        << ",\"iq_blocks\":" << bridge_state.iq_blocks
        << ",\"iq_bytes\":" << bridge_state.iq_bytes
        << ",\"read_errors\":" << bridge_state.read_errors
        << ",\"reconnects\":" << bridge_state.reconnects
        << ",\"uptime_ms\":" << uptime << ",\"supported_gains_tenths_db\":[";
    for (size_t index = 0; index < bridge_state.supported_gains.size(); ++index) {
        if (index) out << ',';
        out << bridge_state.supported_gains[index];
    }
    out << "],\"spectrum\":[";
    for (size_t index = 0; index < bridge_state.spectrum.size(); ++index) {
        if (index) out << ',';
        out << static_cast<unsigned>(bridge_state.spectrum[index]);
    }
    out << "]}";
    return out.str();
}

std::optional<long long> json_integer(std::string_view body, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    size_t position = body.find(needle);
    if (position == std::string_view::npos) return std::nullopt;
    position = body.find(':', position + needle.size());
    if (position == std::string_view::npos) return std::nullopt;
    ++position;
    while (position < body.size() && (body[position] == ' ' || body[position] == '\t')) ++position;
    char* end = nullptr;
    const std::string tail(body.substr(position));
    errno = 0;
    const long long value = std::strtoll(tail.c_str(), &end, 10);
    if (errno != 0 || end == tail.c_str()) return std::nullopt;
    return value;
}

std::optional<bool> json_boolean(std::string_view body, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    size_t position = body.find(needle);
    if (position == std::string_view::npos) return std::nullopt;
    position = body.find(':', position + needle.size());
    if (position == std::string_view::npos) return std::nullopt;
    ++position;
    while (position < body.size() && (body[position] == ' ' || body[position] == '\t')) ++position;
    if (body.substr(position, 4) == "true") return true;
    if (body.substr(position, 5) == "false") return false;
    return std::nullopt;
}

struct HttpResponse {
    int status{200};
    std::string reason{"OK"};
    std::string content_type{"application/json; charset=utf-8"};
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
};

HttpResponse json_error(int status, std::string reason, std::string detail) {
    return {status, std::move(reason), "application/json; charset=utf-8",
            "{\"ok\":false,\"error\":\"" + json_escape(detail) + "\"}"};
}

bool control_rate_allowed() {
    constexpr unsigned kMaximumControlRequestsPerSecond = 30;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(control_rate_mutex);
    if (now - control_window_started >= 1s) {
        control_window_started = now;
        control_requests_in_window = 0;
    }
    if (control_requests_in_window >= kMaximumControlRequestsPerSecond) return false;
    ++control_requests_in_window;
    return true;
}

HttpResponse apply_control(std::string_view body) {
    if (!control_rate_allowed()) {
        auto response = json_error(429, "Too Many Requests", "control rate limit exceeded");
        response.headers.emplace_back("Retry-After", "1");
        return response;
    }
    const auto frequency_value = json_integer(body, "frequency_hz");
    const auto automatic_gain_value = json_boolean(body, "automatic_gain");
    const auto gain_value = json_integer(body, "gain_tenths_db");
    if (!frequency_value && !automatic_gain_value && !gain_value) {
        return json_error(400, "Bad Request", "no supported control field was provided");
    }
    if (frequency_value && (*frequency_value < kMinimumFrequencyHz ||
                            *frequency_value > kMaximumFrequencyHz)) {
        return json_error(422, "Unprocessable Content", "frequency is outside 22.0-948.6 MHz");
    }
    if (gain_value && (*gain_value < -100 || *gain_value > 500)) {
        return json_error(422, "Unprocessable Content", "gain is outside the supported range");
    }
    if (frequency_value) requested_control.frequency_hz.store(static_cast<uint32_t>(*frequency_value));
    if (automatic_gain_value) requested_control.automatic_gain.store(*automatic_gain_value);
    if (gain_value) requested_control.gain_tenths_db.store(static_cast<int>(*gain_value));
    requested_control.revision.fetch_add(1);
    return {202, "Accepted", "application/json; charset=utf-8",
            "{\"ok\":true,\"accepted\":true}"};
}

std::string mime_type(const std::filesystem::path& file) {
    const auto extension = file.extension().string();
    if (extension == ".html") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js") return "application/javascript; charset=utf-8";
    if (extension == ".ttf") return "font/ttf";
    if (extension == ".png") return "image/png";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

std::optional<std::string> read_public_file(const std::filesystem::path& root,
                                            std::string_view request_path,
                                            std::string& content_type) {
    std::string relative(request_path == "/" ? "index.html" : request_path.substr(1));
    if (relative.empty() || relative.find("..") != std::string::npos ||
        relative.front() == '/' || relative.find('\\') != std::string::npos) {
        return std::nullopt;
    }
    const auto file = root / relative;
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) return std::nullopt;
    std::ifstream input(file, std::ios::binary);
    if (!input) return std::nullopt;
    std::ostringstream data;
    data << input.rdbuf();
    content_type = mime_type(file);
    return data.str();
}

HttpResponse route_request(std::string_view method,
                           std::string_view target,
                           std::string_view body,
                           const std::filesystem::path& web_root) {
    const size_t query = target.find('?');
    const std::string_view path = target.substr(0, query);
    if (method == "GET" && path == "/healthz") {
        return {200, "OK", "text/plain; charset=utf-8", "ok\n"};
    }
    if (method == "GET" && path == "/api/status") {
        return {200, "OK", "application/json; charset=utf-8", status_json(),
                {{"Cache-Control", "no-store"}}};
    }
    if (method == "POST" && path == "/api/control") return apply_control(body);
    if (method == "OPTIONS" && path.starts_with("/api/")) {
        return {204, "No Content", "text/plain", "", {{"Allow", "GET, POST, OPTIONS"}}};
    }
    if (method != "GET") return json_error(405, "Method Not Allowed", "method not allowed");

    std::string content_type;
    const auto data = read_public_file(web_root, path, content_type);
    if (!data) return json_error(404, "Not Found", "resource not found");
    return {200, "OK", std::move(content_type), *data,
            {{"Cache-Control", path == "/" || path == "/index.html"
                                   ? "no-cache"
                                   : "public, max-age=604800"}}};
}

void send_response(int client, const HttpResponse& response) {
    std::ostringstream header;
    header << "HTTP/1.1 " << response.status << ' ' << response.reason << "\r\n"
           << "Content-Type: " << response.content_type << "\r\n"
           << "Content-Length: " << response.body.size() << "\r\n"
           << "Connection: close\r\n"
           << "X-Content-Type-Options: nosniff\r\n"
           << "Referrer-Policy: strict-origin-when-cross-origin\r\n"
           << "X-Frame-Options: SAMEORIGIN\r\n"
           << "Permissions-Policy: camera=(), microphone=(), geolocation=()\r\n";
    for (const auto& [name, value] : response.headers) header << name << ": " << value << "\r\n";
    header << "\r\n";
    const std::string head = header.str();
    auto write_all = [client](std::string_view data) {
        while (!data.empty()) {
            const ssize_t written = ::send(client, data.data(), data.size(), MSG_NOSIGNAL);
            if (written <= 0) return;
            data.remove_prefix(static_cast<size_t>(written));
        }
    };
    write_all(head);
    write_all(response.body);
}

void handle_client(int client, const std::filesystem::path& web_root) {
    timeval timeout{5, 0};
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    std::string request;
    std::array<char, 4096> chunk{};
    size_t expected_size = 0;
    while (request.size() < 64 * 1024) {
        const ssize_t received = recv(client, chunk.data(), chunk.size(), 0);
        if (received <= 0) break;
        request.append(chunk.data(), static_cast<size_t>(received));
        const size_t headers_end = request.find("\r\n\r\n");
        if (headers_end != std::string::npos) {
            if (expected_size == 0) {
                const std::string_view headers(request.data(), headers_end);
                const size_t length_header = headers.find("Content-Length:");
                size_t body_length = 0;
                if (length_header != std::string_view::npos) {
                    const size_t value_start = length_header + std::strlen("Content-Length:");
                    body_length = static_cast<size_t>(std::strtoull(
                        std::string(headers.substr(value_start)).c_str(), nullptr, 10));
                }
                expected_size = headers_end + 4 + body_length;
            }
            if (request.size() >= expected_size) break;
        }
    }

    const size_t line_end = request.find("\r\n");
    if (line_end == std::string::npos) {
        send_response(client, json_error(400, "Bad Request", "malformed request"));
        return;
    }
    std::istringstream first_line(request.substr(0, line_end));
    std::string method;
    std::string target;
    std::string version;
    first_line >> method >> target >> version;
    const size_t headers_end = request.find("\r\n\r\n");
    const std::string_view body = headers_end == std::string::npos
        ? std::string_view{}
        : std::string_view(request).substr(headers_end + 4);
    send_response(client, route_request(method, target, body, web_root));
}

int parse_port() {
    const char* value = std::getenv("ZERO_SDR_BRIDGE_PORT");
    if (!value || !*value) return kDefaultPort;
    char* end = nullptr;
    const long port = std::strtol(value, &end, 10);
    if (!end || *end != '\0' || port < 1024 || port > 65535) return -1;
    return static_cast<int>(port);
}

} // namespace

int main() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    const int port = parse_port();
    if (port < 0) {
        std::cerr << "invalid ZERO_SDR_BRIDGE_PORT\n";
        return 2;
    }
    const char* root_env = std::getenv("ZERO_SDR_WEB_ROOT");
    const std::filesystem::path web_root = root_env && *root_env
        ? std::filesystem::path(root_env)
        : std::filesystem::current_path() / "web-simulator";
    if (!std::filesystem::is_regular_file(web_root / "index.html")) {
        std::cerr << "web root does not contain index.html: " << web_root << '\n';
        return 2;
    }

    const int server = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (server < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << '\n';
        return 1;
    }
    const int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(server, 32) != 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << '\n';
        close(server);
        return 1;
    }

    std::thread receiver(receiver_loop);
    std::cout << "zero-sdr bridge listening on 0.0.0.0:" << port
              << " with web root " << web_root << '\n';

    while (running.load()) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(server, &read_set);
        timeval timeout{0, 250'000};
        const int ready = select(server + 1, &read_set, nullptr, nullptr, &timeout);
        if (ready <= 0) continue;
        const int client = accept4(server, nullptr, nullptr, SOCK_CLOEXEC);
        if (client < 0) continue;
        std::thread([client, web_root] {
            handle_client(client, web_root);
            close(client);
        }).detach();
    }

    close(server);
    if (receiver.joinable()) receiver.join();
    return 0;
}
