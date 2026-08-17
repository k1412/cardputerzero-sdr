// SPDX-License-Identifier: MIT

#include "device/rtl_sdr_device.h"
#include "dsp/iq_spectrum.h"
#include "dsp/wfm_demodulator.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
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
#include <dlfcn.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

using namespace std::chrono_literals;

constexpr uint32_t kMinimumFrequencyHz = 22'000'000;
constexpr uint32_t kMaximumFrequencyHz = 948'600'000;
constexpr uint32_t kDefaultFrequencyHz = 106'100'000;
constexpr uint32_t kSampleRateHz = 2'048'000;
constexpr uint32_t kBroadcastMinimumFrequencyHz = 76'000'000;
constexpr uint32_t kBroadcastMaximumFrequencyHz = 108'000'000;
constexpr size_t kIqBlockBytes = 16 * 16'384;
constexpr size_t kAudioRingSamples = dsp::WfmDemodulator::kAudioSampleRateHz * 3;
constexpr size_t kMaximumAudioResponseSamples = 8'192;
constexpr size_t kMp3RingBytes = 256 * 1'024;
constexpr uint16_t kDefaultPort = 18'117;

std::atomic_bool running{true};
std::mutex control_rate_mutex;
std::chrono::steady_clock::time_point control_window_started{
    std::chrono::steady_clock::now()};
unsigned control_requests_in_window{0};
std::mutex audio_rate_mutex;
std::chrono::steady_clock::time_point audio_window_started{
    std::chrono::steady_clock::now()};
unsigned audio_requests_in_window{0};
std::atomic_uint active_mp3_streams{0};
std::condition_variable audio_condition;

void handle_signal(int) {
    running.store(false);
    audio_condition.notify_all();
}

class Mp3Encoder {
public:
    ~Mp3Encoder() {
        close_encoder();
        if (library_) dlclose(library_);
    }

    bool reset(std::string& error) {
        close_encoder();
        if (!load_library(error)) return false;
        encoder_ = lame_init_();
        if (!encoder_) {
            error = "lame_init failed";
            return false;
        }
        const bool configured =
            lame_set_in_samplerate_(encoder_, dsp::WfmDemodulator::kAudioSampleRateHz) == 0 &&
            lame_set_out_samplerate_(encoder_, dsp::WfmDemodulator::kAudioSampleRateHz) == 0 &&
            lame_set_num_channels_(encoder_, 1) == 0 &&
            lame_set_mode_(encoder_, 3) == 0 &&
            lame_set_brate_(encoder_, 64) == 0 &&
            lame_set_quality_(encoder_, 5) == 0 &&
            lame_init_params_(encoder_) == 0;
        if (!configured) {
            error = "libmp3lame configuration failed";
            close_encoder();
            return false;
        }
        error.clear();
        return true;
    }

    std::vector<uint8_t> encode(const std::vector<int16_t>& samples, std::string& error) {
        if (!encoder_ || samples.empty()) return {};
        std::vector<uint8_t> output(static_cast<size_t>(
            std::ceil(1.25 * static_cast<double>(samples.size()))) + 7'200);
        const int encoded = lame_encode_buffer_(
            encoder_, samples.data(), samples.data(), static_cast<int>(samples.size()),
            output.data(), static_cast<int>(output.size()));
        if (encoded < 0) {
            error = "libmp3lame encode failed: " + std::to_string(encoded);
            return {};
        }
        output.resize(static_cast<size_t>(encoded));
        return output;
    }

private:
    using Encoder = void*;
    using InitFn = Encoder (*)();
    using SetIntFn = int (*)(Encoder, int);
    using InitParamsFn = int (*)(Encoder);
    using EncodeFn = int (*)(Encoder, const short*, const short*, int, unsigned char*, int);
    using CloseFn = int (*)(Encoder);

    template <typename Function>
    bool load_symbol(Function& target, const char* name, std::string& error) {
        target = reinterpret_cast<Function>(dlsym(library_, name));
        if (target) return true;
        error = std::string("missing libmp3lame symbol: ") + name;
        return false;
    }

    bool load_library(std::string& error) {
        if (library_) return true;
        library_ = dlopen("libmp3lame.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!library_) {
            const char* detail = dlerror();
            error = detail ? detail : "libmp3lame is unavailable";
            return false;
        }
        return load_symbol(lame_init_, "lame_init", error) &&
               load_symbol(lame_set_in_samplerate_, "lame_set_in_samplerate", error) &&
               load_symbol(lame_set_out_samplerate_, "lame_set_out_samplerate", error) &&
               load_symbol(lame_set_num_channels_, "lame_set_num_channels", error) &&
               load_symbol(lame_set_mode_, "lame_set_mode", error) &&
               load_symbol(lame_set_brate_, "lame_set_brate", error) &&
               load_symbol(lame_set_quality_, "lame_set_quality", error) &&
               load_symbol(lame_init_params_, "lame_init_params", error) &&
               load_symbol(lame_encode_buffer_, "lame_encode_buffer", error) &&
               load_symbol(lame_close_, "lame_close", error);
    }

    void close_encoder() {
        if (encoder_ && lame_close_) lame_close_(encoder_);
        encoder_ = nullptr;
    }

    void* library_{nullptr};
    Encoder encoder_{nullptr};
    InitFn lame_init_{nullptr};
    SetIntFn lame_set_in_samplerate_{nullptr};
    SetIntFn lame_set_out_samplerate_{nullptr};
    SetIntFn lame_set_num_channels_{nullptr};
    SetIntFn lame_set_mode_{nullptr};
    SetIntFn lame_set_brate_{nullptr};
    SetIntFn lame_set_quality_{nullptr};
    InitParamsFn lame_init_params_{nullptr};
    EncodeFn lame_encode_buffer_{nullptr};
    CloseFn lame_close_{nullptr};
};

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

struct Mp3Chunk {
    uint64_t start_byte{0};
    std::vector<uint8_t> data;
};

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
    std::deque<int16_t> audio_samples;
    uint64_t audio_start_sample{0};
    uint64_t audio_end_sample{0};
    uint64_t audio_chunks{0};
    uint64_t audio_epoch{1};
    uint32_t audio_peak{0};
    uint32_t audio_rms{0};
    uint64_t audio_http_requests{0};
    uint64_t audio_http_bytes{0};
    std::chrono::steady_clock::time_point audio_last_request_at{};
    std::deque<Mp3Chunk> mp3_chunks;
    uint64_t mp3_start_byte{0};
    uint64_t mp3_end_byte{0};
    size_t mp3_buffered_bytes{0};
    bool mp3_available{false};
    std::string mp3_error;
    uint64_t mp3_stream_connections{0};
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

bool is_broadcast_frequency(uint32_t frequency_hz) {
    return frequency_hz >= kBroadcastMinimumFrequencyHz &&
           frequency_hz <= kBroadcastMaximumFrequencyHz;
}

void reset_audio_locked() {
    bridge_state.audio_samples.clear();
    bridge_state.audio_start_sample = bridge_state.audio_end_sample;
    bridge_state.audio_peak = 0;
    bridge_state.audio_rms = 0;
    bridge_state.mp3_chunks.clear();
    bridge_state.mp3_start_byte = bridge_state.mp3_end_byte;
    bridge_state.mp3_buffered_bytes = 0;
    ++bridge_state.audio_epoch;
}

void append_audio_locked(const std::vector<int16_t>& samples) {
    if (samples.empty()) return;
    uint32_t peak = 0;
    long double square_sum = 0.0L;
    for (const int16_t sample : samples) {
        const int32_t magnitude = std::abs(static_cast<int32_t>(sample));
        peak = std::max(peak, static_cast<uint32_t>(magnitude));
        square_sum += static_cast<long double>(sample) * static_cast<long double>(sample);
        bridge_state.audio_samples.push_back(sample);
    }
    bridge_state.audio_end_sample += samples.size();
    ++bridge_state.audio_chunks;
    bridge_state.audio_peak = peak;
    bridge_state.audio_rms = static_cast<uint32_t>(std::lround(
        std::sqrt(square_sum / static_cast<long double>(samples.size()))));
    while (bridge_state.audio_samples.size() > kAudioRingSamples) {
        bridge_state.audio_samples.pop_front();
        ++bridge_state.audio_start_sample;
    }
}

void append_mp3_locked(std::vector<uint8_t> bytes) {
    if (bytes.empty()) return;
    const size_t size = bytes.size();
    bridge_state.mp3_chunks.push_back({bridge_state.mp3_end_byte, std::move(bytes)});
    bridge_state.mp3_end_byte += size;
    bridge_state.mp3_buffered_bytes += size;
    while (bridge_state.mp3_buffered_bytes > kMp3RingBytes &&
           bridge_state.mp3_chunks.size() > 1) {
        bridge_state.mp3_buffered_bytes -= bridge_state.mp3_chunks.front().data.size();
        bridge_state.mp3_chunks.pop_front();
    }
    bridge_state.mp3_start_byte = bridge_state.mp3_chunks.empty()
        ? bridge_state.mp3_end_byte
        : bridge_state.mp3_chunks.front().start_byte;
}

void publish_status(std::string status, std::string detail = {}, std::string device_name = {}) {
    {
        std::lock_guard lock(bridge_state.mutex);
        if (status != "live" && bridge_state.status == "live") reset_audio_locked();
        bridge_state.status = std::move(status);
        bridge_state.detail = std::move(detail);
        if (!device_name.empty()) bridge_state.device_name = std::move(device_name);
    }
    audio_condition.notify_all();
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

        Mp3Encoder mp3_encoder;
        std::string mp3_error;
        bool mp3_ready = mp3_encoder.reset(mp3_error);
        {
            std::lock_guard lock(bridge_state.mutex);
            bridge_state.status = "live";
            bridge_state.detail.clear();
            bridge_state.device_name = devices.front().name;
            bridge_state.frequency_hz = initial_frequency;
            bridge_state.automatic_gain = initial_auto_gain;
            bridge_state.gain_tenths_db = initial_gain;
            bridge_state.supported_gains = gains;
            reset_audio_locked();
            bridge_state.mp3_available = mp3_ready;
            bridge_state.mp3_error = mp3_error;
            ++bridge_state.reconnects;
        }
        audio_condition.notify_all();

        dsp::IqSpectrum spectrum_processor;
        dsp::WfmDemodulator audio_processor;
        std::vector<uint8_t> iq(kIqBlockBytes);
        uint64_t applied_revision = requested_control.revision.load();
        uint32_t applied_frequency = initial_frequency;

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
                    reset_audio_locked();
                }
                audio_processor.reset();
                mp3_ready = mp3_encoder.reset(mp3_error);
                {
                    std::lock_guard lock(bridge_state.mutex);
                    bridge_state.mp3_available = mp3_ready;
                    bridge_state.mp3_error = mp3_error;
                }
                audio_condition.notify_all();
                applied_frequency = frequency;
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
            const auto audio = is_broadcast_frequency(applied_frequency)
                ? audio_processor.process(iq.data(), bytes_read)
                : std::vector<int16_t>{};
            const auto mp3 = mp3_ready ? mp3_encoder.encode(audio, mp3_error)
                                       : std::vector<uint8_t>{};
            {
                std::lock_guard lock(bridge_state.mutex);
                bridge_state.status = "live";
                bridge_state.detail.clear();
                bridge_state.spectrum = frame.level;
                bridge_state.frame_sequence = frame.sequence;
                ++bridge_state.iq_blocks;
                bridge_state.iq_bytes += bytes_read;
                append_audio_locked(audio);
                append_mp3_locked(mp3);
                if (!mp3_error.empty()) bridge_state.mp3_error = mp3_error;
            }
            if (!mp3.empty()) audio_condition.notify_all();
        }

        radio.close();
        if (running.load()) interruptible_pause(1s);
    }
}

std::string status_json() {
    std::lock_guard lock(bridge_state.mutex);
    const auto now = std::chrono::steady_clock::now();
    const auto uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - bridge_state.started_at).count();
    const auto audio_request_age = bridge_state.audio_http_requests == 0
        ? -1LL
        : std::chrono::duration_cast<std::chrono::milliseconds>(
              now - bridge_state.audio_last_request_at).count();
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
        << ",\"audio_available\":"
        << (bridge_state.status == "live" &&
            is_broadcast_frequency(bridge_state.frequency_hz) &&
            !bridge_state.audio_samples.empty() ? "true" : "false")
        << ",\"audio_sample_rate_hz\":" << dsp::WfmDemodulator::kAudioSampleRateHz
        << ",\"audio_epoch\":" << bridge_state.audio_epoch
        << ",\"audio_start_sample\":" << bridge_state.audio_start_sample
        << ",\"audio_end_sample\":" << bridge_state.audio_end_sample
        << ",\"audio_buffered_samples\":" << bridge_state.audio_samples.size()
        << ",\"audio_chunks\":" << bridge_state.audio_chunks
        << ",\"audio_peak\":" << bridge_state.audio_peak
        << ",\"audio_rms\":" << bridge_state.audio_rms
        << ",\"audio_http_requests\":" << bridge_state.audio_http_requests
        << ",\"audio_http_bytes\":" << bridge_state.audio_http_bytes
        << ",\"audio_last_request_age_ms\":" << audio_request_age
        << ",\"mp3_available\":" << (bridge_state.mp3_available ? "true" : "false")
        << ",\"mp3_error\":\"" << json_escape(bridge_state.mp3_error) << "\""
        << ",\"mp3_buffered_bytes\":" << bridge_state.mp3_buffered_bytes
        << ",\"mp3_stream_connections\":" << bridge_state.mp3_stream_connections
        << ",\"mp3_active_streams\":" << active_mp3_streams.load()
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
            "{\"ok\":false,\"error\":\"" + json_escape(detail) + "\"}", {}};
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

bool audio_rate_allowed() {
    constexpr unsigned kMaximumAudioRequestsPerSecond = 120;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(audio_rate_mutex);
    if (now - audio_window_started >= 1s) {
        audio_window_started = now;
        audio_requests_in_window = 0;
    }
    if (audio_requests_in_window >= kMaximumAudioRequestsPerSecond) return false;
    ++audio_requests_in_window;
    return true;
}

std::optional<uint64_t> query_unsigned(std::string_view target, std::string_view key) {
    const size_t query = target.find('?');
    if (query == std::string_view::npos) return std::nullopt;
    const std::string needle = std::string(key) + "=";
    size_t position = query + 1;
    while (position < target.size()) {
        const size_t end = target.find('&', position);
        const std::string_view field = target.substr(
            position, end == std::string_view::npos ? target.size() - position : end - position);
        if (field.starts_with(needle)) {
            const std::string value(field.substr(needle.size()));
            if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char ch) {
                    return ch >= '0' && ch <= '9';
                })) {
                return std::nullopt;
            }
            char* parse_end = nullptr;
            errno = 0;
            const unsigned long long parsed = std::strtoull(value.c_str(), &parse_end, 10);
            if (errno != 0 || !parse_end || *parse_end != '\0') return std::nullopt;
            return static_cast<uint64_t>(parsed);
        }
        if (end == std::string_view::npos) break;
        position = end + 1;
    }
    return std::nullopt;
}

bool query_contains(std::string_view target, std::string_view key) {
    const size_t query = target.find('?');
    if (query == std::string_view::npos) return false;
    const std::string needle = std::string(key) + "=";
    size_t position = query + 1;
    while (position < target.size()) {
        const size_t end = target.find('&', position);
        const std::string_view field = target.substr(
            position, end == std::string_view::npos ? target.size() - position : end - position);
        if (field.starts_with(needle)) return true;
        if (end == std::string_view::npos) break;
        position = end + 1;
    }
    return false;
}

HttpResponse audio_response(std::string_view target) {
    if (!audio_rate_allowed()) {
        auto response = json_error(429, "Too Many Requests", "audio request rate limit exceeded");
        response.headers.emplace_back("Retry-After", "1");
        return response;
    }
    const auto after = query_unsigned(target, "after");
    if (query_contains(target, "after") && !after) {
        return json_error(400, "Bad Request", "invalid audio cursor");
    }

    std::lock_guard lock(bridge_state.mutex);
    ++bridge_state.audio_http_requests;
    bridge_state.audio_last_request_at = std::chrono::steady_clock::now();
    if (bridge_state.status != "live") {
        return json_error(409, "Conflict", "RTL-SDR is not ready");
    }
    if (!is_broadcast_frequency(bridge_state.frequency_hz)) {
        return json_error(422, "Unprocessable Content",
                          "browser audio is limited to the 76-108 MHz broadcast band");
    }

    uint64_t start = after.value_or(
        bridge_state.audio_end_sample > 4'096
            ? bridge_state.audio_end_sample - 4'096
            : bridge_state.audio_start_sample);
    start = std::max(start, bridge_state.audio_start_sample);
    start = std::min(start, bridge_state.audio_end_sample);
    const size_t count = static_cast<size_t>(std::min<uint64_t>(
        bridge_state.audio_end_sample - start, kMaximumAudioResponseSamples));
    const uint64_t end = start + count;
    const std::vector<std::pair<std::string, std::string>> headers{
        {"Cache-Control", "no-store"},
        {"X-Audio-Format", "s16le-mono"},
        {"X-Audio-Sample-Rate", std::to_string(dsp::WfmDemodulator::kAudioSampleRateHz)},
        {"X-Audio-Epoch", std::to_string(bridge_state.audio_epoch)},
        {"X-Audio-Start-Sample", std::to_string(start)},
        {"X-Audio-End-Sample", std::to_string(end)},
    };
    if (count == 0) {
        return {204, "No Content", "application/octet-stream", "", headers};
    }

    std::string body(count * sizeof(int16_t), '\0');
    const size_t offset = static_cast<size_t>(start - bridge_state.audio_start_sample);
    for (size_t index = 0; index < count; ++index) {
        const uint16_t sample = static_cast<uint16_t>(bridge_state.audio_samples[offset + index]);
        body[index * 2] = static_cast<char>(sample & 0xffU);
        body[index * 2 + 1] = static_cast<char>((sample >> 8U) & 0xffU);
    }
    bridge_state.audio_http_bytes += body.size();
    return {200, "OK", "application/octet-stream", std::move(body), headers};
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
            "{\"ok\":true,\"accepted\":true}", {}};
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
        return {200, "OK", "text/plain; charset=utf-8", "ok\n", {}};
    }
    if (method == "GET" && path == "/api/status") {
        return {200, "OK", "application/json; charset=utf-8", status_json(),
                {{"Cache-Control", "no-store"}}};
    }
    if (method == "GET" && path == "/api/audio") return audio_response(target);
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

bool write_all(int client, std::string_view data) {
    while (!data.empty()) {
        const ssize_t written = ::send(client, data.data(), data.size(), MSG_NOSIGNAL);
        if (written <= 0) return false;
        data.remove_prefix(static_cast<size_t>(written));
    }
    return true;
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
    if (write_all(client, head)) write_all(client, response.body);
}

void stream_mp3(int client) {
    constexpr unsigned kMaximumConcurrentStreams = 4;
    const unsigned previous = active_mp3_streams.fetch_add(1);
    if (previous >= kMaximumConcurrentStreams) {
        active_mp3_streams.fetch_sub(1);
        send_response(client, json_error(429, "Too Many Requests", "too many audio streams"));
        return;
    }
    struct StreamGuard {
        ~StreamGuard() { active_mp3_streams.fetch_sub(1); }
    } guard;

    uint64_t epoch = 0;
    uint64_t cursor = 0;
    {
        std::lock_guard lock(bridge_state.mutex);
        if (bridge_state.status != "live" ||
            !is_broadcast_frequency(bridge_state.frequency_hz)) {
            send_response(client, json_error(409, "Conflict", "WFM radio is not ready"));
            return;
        }
        if (!bridge_state.mp3_available) {
            send_response(client, json_error(503, "Service Unavailable",
                                             bridge_state.mp3_error.empty()
                                                 ? "MP3 encoder is unavailable"
                                                 : bridge_state.mp3_error));
            return;
        }
        epoch = bridge_state.audio_epoch;
        const uint64_t target = bridge_state.mp3_end_byte > 8'192
            ? bridge_state.mp3_end_byte - 8'192
            : bridge_state.mp3_start_byte;
        cursor = bridge_state.mp3_start_byte;
        for (const auto& chunk : bridge_state.mp3_chunks) {
            if (chunk.start_byte + chunk.data.size() > target) {
                cursor = chunk.start_byte;
                break;
            }
        }
        ++bridge_state.mp3_stream_connections;
    }

    const std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: audio/mpeg\r\n"
        "Cache-Control: no-store, no-cache\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "Accept-Ranges: none\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "icy-name: Zero SDR LAN Radio\r\n"
        "icy-br: 64\r\n\r\n";
    if (!write_all(client, header)) return;

    while (running.load()) {
        std::string batch;
        {
            std::unique_lock lock(bridge_state.mutex);
            audio_condition.wait_for(lock, 1s, [&] {
                return !running.load() || bridge_state.audio_epoch != epoch ||
                       bridge_state.status != "live" ||
                       bridge_state.mp3_end_byte > cursor;
            });
            if (!running.load() || bridge_state.audio_epoch != epoch ||
                bridge_state.status != "live") {
                break;
            }
            if (cursor < bridge_state.mp3_start_byte) cursor = bridge_state.mp3_start_byte;
            for (const auto& chunk : bridge_state.mp3_chunks) {
                const uint64_t chunk_end = chunk.start_byte + chunk.data.size();
                if (chunk_end <= cursor) continue;
                const size_t offset = cursor > chunk.start_byte
                    ? static_cast<size_t>(cursor - chunk.start_byte)
                    : 0;
                batch.append(reinterpret_cast<const char*>(chunk.data.data() + offset),
                             chunk.data.size() - offset);
                cursor = chunk_end;
            }
        }
        if (!batch.empty()) {
            std::ostringstream prefix;
            prefix << std::hex << batch.size() << "\r\n";
            if (!write_all(client, prefix.str()) || !write_all(client, batch) ||
                !write_all(client, "\r\n")) {
                return;
            }
        }
    }
    write_all(client, "0\r\n\r\n");
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
    if (request.empty()) return;
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
    const size_t query = target.find('?');
    const std::string_view path(target.data(), query == std::string::npos ? target.size() : query);
    if (method == "GET" && path == "/api/radio.mp3") {
        stream_mp3(client);
        return;
    }
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
