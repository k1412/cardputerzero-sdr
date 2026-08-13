// SPDX-License-Identifier: MIT

#include "radio_session.h"

#include "alsa_audio_sink.h"
#include "iq_spectrum.h"
#include "rtl_sdr_device.h"
#include "wfm_demodulator.h"

#include <array>
#include <chrono>
#include <utility>

namespace device {
namespace {

constexpr uint32_t kSampleRateHz = 2'048'000;
constexpr size_t kReadBufferBytes = 16'384;
constexpr auto kReconnectDelay = std::chrono::seconds(2);

} // namespace

RadioSession::RadioSession(std::string library_path,
                           std::string audio_library_path,
                           std::string audio_device_name)
    : library_path_(std::move(library_path)),
      audio_library_path_(std::move(audio_library_path)),
      audio_device_name_(std::move(audio_device_name)) {}

RadioSession::~RadioSession() {
    stop();
}

void RadioSession::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    set_state(RadioSessionState::Connecting);
    worker_ = std::thread(&RadioSession::worker_main, this);
}

void RadioSession::stop() {
    if (!running_.exchange(false)) return;
    stop_condition_.notify_all();
    if (worker_.joinable()) worker_.join();
    set_state(RadioSessionState::Stopped);
}

bool RadioSession::started() const {
    return running_.load();
}

void RadioSession::request_frequency(uint32_t frequency_hz) {
    requested_frequency_hz_.store(frequency_hz);
    settings_generation_.fetch_add(1);
}

void RadioSession::request_gain(bool automatic_gain, int gain_tenths_db) {
    requested_automatic_gain_.store(automatic_gain);
    requested_gain_tenths_db_.store(gain_tenths_db);
    settings_generation_.fetch_add(1);
}

void RadioSession::request_muted(bool muted) {
    requested_muted_.store(muted);
}

RadioSessionState RadioSession::state() const {
    return state_.load();
}

std::string RadioSession::status_detail() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return status_detail_;
}

std::string RadioSession::device_name() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return device_name_;
}

bool RadioSession::latest_spectrum(dsp::SpectrumFrame& frame) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    if (latest_frame_.sequence == 0) return false;
    frame = latest_frame_;
    return true;
}

bool RadioSession::audio_active() const {
    return audio_active_.load();
}

void RadioSession::worker_main() {
    RtlSdrDevice radio(library_path_);
    dsp::IqSpectrum spectrum;
    dsp::WfmDemodulator demodulator;
    audio::AlsaAudioSink audio_sink(audio_library_path_);
    std::array<uint8_t, kReadBufferBytes> iq_buffer{};

    while (running_.load()) {
        set_state(RadioSessionState::Connecting);
        if (!radio.library_available()) {
            set_state(RadioSessionState::Missing, radio.library_error());
            if (wait_for_retry()) break;
            continue;
        }

        const auto available_devices = radio.devices();
        if (available_devices.empty()) {
            set_state(RadioSessionState::Missing, "no RTL-SDR USB device detected");
            if (wait_for_retry()) break;
            continue;
        }

        std::string error;
        if (!radio.open(available_devices.front().index, error) ||
            !radio.configure(requested_frequency_hz_.load(),
                             kSampleRateHz,
                             requested_automatic_gain_.load(),
                             requested_gain_tenths_db_.load(),
                             error)) {
            radio.close();
            set_state(RadioSessionState::Error, error, available_devices.front().name);
            if (wait_for_retry()) break;
            continue;
        }

        uint64_t applied_generation = settings_generation_.load();
        std::string audio_error;
        audio_active_.store(audio_sink.open(audio_device_name_,
                                            dsp::WfmDemodulator::kAudioSampleRateHz,
                                            audio_error));
        audio_sink.set_muted(requested_muted_.load());
        demodulator.reset();
        set_state(RadioSessionState::Live, audio_error, available_devices.front().name);
        while (running_.load() && radio.is_open()) {
            audio_sink.set_muted(requested_muted_.load());
            const uint64_t requested_generation = settings_generation_.load();
            if (requested_generation != applied_generation) {
                if (!radio.set_center_frequency(requested_frequency_hz_.load(), error) ||
                    !radio.set_gain(requested_automatic_gain_.load(),
                                    requested_gain_tenths_db_.load(),
                                    error) ||
                    !radio.reset_buffer(error)) {
                    set_state(RadioSessionState::Error, error, available_devices.front().name);
                    break;
                }
                demodulator.reset();
                applied_generation = requested_generation;
            }

            size_t bytes_read = 0;
            if (!radio.read_sync(iq_buffer.data(), iq_buffer.size(), bytes_read, error) ||
                bytes_read < dsp::kSpectrumBinCount * 2) {
                set_state(RadioSessionState::Error,
                          error.empty() ? "RTL-SDR returned a short IQ block" : error,
                          available_devices.front().name);
                break;
            }

            auto frame = spectrum.process(iq_buffer.data(), bytes_read);
            const auto audio = demodulator.process(iq_buffer.data(), bytes_read);
            if (audio_active_.load() && !audio.empty()) {
                audio_sink.submit(audio.data(), audio.size());
            }
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                latest_frame_ = frame;
            }
            std::this_thread::yield();
        }
        audio_sink.close();
        audio_active_.store(false);
        radio.close();
        if (running_.load() && wait_for_retry()) break;
    }
    audio_sink.close();
    audio_active_.store(false);
}

bool RadioSession::wait_for_retry() {
    std::unique_lock<std::mutex> lock(data_mutex_);
    return stop_condition_.wait_for(lock, kReconnectDelay, [this] { return !running_.load(); });
}

void RadioSession::set_state(RadioSessionState state,
                             std::string detail,
                             std::string device_name) {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        status_detail_ = std::move(detail);
        if (!device_name.empty() || state != RadioSessionState::Live) {
            device_name_ = std::move(device_name);
        }
        if (state != RadioSessionState::Live) {
            latest_frame_ = {};
        }
    }
    state_.store(state);
}

} // namespace device
