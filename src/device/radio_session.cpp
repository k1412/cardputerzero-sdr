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
    reset_metrics();
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

std::vector<int> RadioSession::supported_gains() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return supported_gains_;
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

RadioSessionMetrics RadioSession::metrics() const {
    return {
        metric_connection_attempts_.load(),
        metric_successful_connections_.load(),
        metric_retry_waits_.load(),
        metric_read_errors_.load(),
        metric_settings_updates_.load(),
        metric_iq_blocks_.load(),
        metric_iq_bytes_.load(),
        metric_audio_frames_generated_.load(),
        metric_audio_frames_written_.load(),
        metric_audio_frames_dropped_.load(),
        metric_audio_recoveries_.load(),
        metric_audio_write_errors_.load(),
        metric_audio_open_failures_.load(),
        metric_total_processing_us_.load(),
        metric_maximum_processing_us_.load(),
    };
}

void RadioSession::worker_main() {
    RtlSdrDevice radio(library_path_);
    dsp::IqSpectrum spectrum;
    dsp::WfmDemodulator demodulator;
    audio::AlsaAudioSink audio_sink(audio_library_path_);
    std::array<uint8_t, kReadBufferBytes> iq_buffer{};

    while (running_.load()) {
        metric_connection_attempts_.fetch_add(1);
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
        const auto open_result = radio.open(available_devices.front().index, error);
        if (open_result != RtlSdrOpenResult::Opened) {
            radio.close();
            switch (open_result) {
                case RtlSdrOpenResult::AccessDenied:
                    set_state(RadioSessionState::AccessDenied,
                              error,
                              available_devices.front().name);
                    break;
                case RtlSdrOpenResult::Busy:
                    set_state(RadioSessionState::Busy,
                              error,
                              available_devices.front().name);
                    break;
                case RtlSdrOpenResult::Disconnected:
                    set_state(RadioSessionState::Missing,
                              error,
                              available_devices.front().name);
                    break;
                case RtlSdrOpenResult::Failed:
                case RtlSdrOpenResult::Opened:
                    set_state(RadioSessionState::Error,
                              error,
                              available_devices.front().name);
                    break;
            }
            if (wait_for_retry()) break;
            continue;
        }
        const auto tuner_gains = radio.tuner_gains(error);
        if (tuner_gains.empty() ||
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
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            supported_gains_ = tuner_gains;
        }
        metric_successful_connections_.fetch_add(1);

        uint64_t applied_generation = settings_generation_.load();
        const uint64_t audio_written_base = metric_audio_frames_written_.load();
        const uint64_t audio_dropped_base = metric_audio_frames_dropped_.load();
        const uint64_t audio_recoveries_base = metric_audio_recoveries_.load();
        const uint64_t audio_write_errors_base = metric_audio_write_errors_.load();
        std::string audio_error;
        const bool audio_opened = audio_sink.open(audio_device_name_,
                                                  dsp::WfmDemodulator::kAudioSampleRateHz,
                                                  audio_error);
        audio_active_.store(audio_opened);
        if (!audio_opened) metric_audio_open_failures_.fetch_add(1);
        const auto publish_audio_metrics = [&] {
            if (!audio_opened) return;
            metric_audio_frames_written_.store(audio_written_base + audio_sink.frames_written());
            metric_audio_frames_dropped_.store(audio_dropped_base + audio_sink.dropped_frames());
            metric_audio_recoveries_.store(audio_recoveries_base + audio_sink.recoveries());
            metric_audio_write_errors_.store(audio_write_errors_base + audio_sink.write_errors());
        };
        audio_sink.set_muted(requested_muted_.load());
        demodulator.reset();
        bool demodulation_active = false;
        set_state(RadioSessionState::Live, audio_error, available_devices.front().name);
        while (running_.load() && radio.is_open()) {
            const bool muted = requested_muted_.load();
            audio_sink.set_muted(muted);
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
                metric_settings_updates_.fetch_add(1);
            }

            size_t bytes_read = 0;
            if (!radio.read_sync(iq_buffer.data(), iq_buffer.size(), bytes_read, error) ||
                bytes_read < dsp::kSpectrumBinCount * 2) {
                metric_read_errors_.fetch_add(1);
                set_state(RadioSessionState::Error,
                          error.empty() ? "RTL-SDR returned a short IQ block" : error,
                          available_devices.front().name);
                break;
            }

            metric_iq_blocks_.fetch_add(1);
            metric_iq_bytes_.fetch_add(bytes_read);
            const auto processing_started = std::chrono::steady_clock::now();
            auto frame = spectrum.process(iq_buffer.data(), bytes_read);
            const bool should_demodulate = audio_active_.load() && !muted;
            if (should_demodulate) {
                if (!demodulation_active) demodulator.reset();
                demodulation_active = true;
                const auto audio = demodulator.process(iq_buffer.data(), bytes_read);
                metric_audio_frames_generated_.fetch_add(audio.size());
                if (!audio.empty() && !audio_sink.submit(audio.data(), audio.size())) {
                    audio_active_.store(false);
                    demodulation_active = false;
                    set_state(RadioSessionState::Live,
                              "ALSA audio output stopped after an unrecoverable write error",
                              available_devices.front().name);
                }
            }
            else {
                demodulation_active = false;
            }
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                latest_frame_ = frame;
            }
            publish_audio_metrics();
            const auto processing_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - processing_started).count();
            record_processing_time(static_cast<uint64_t>(processing_us));
            std::this_thread::yield();
        }
        audio_sink.close();
        publish_audio_metrics();
        audio_active_.store(false);
        radio.close();
        if (running_.load() && wait_for_retry()) break;
    }
    audio_sink.close();
    audio_active_.store(false);
}

bool RadioSession::wait_for_retry() {
    metric_retry_waits_.fetch_add(1);
    std::unique_lock<std::mutex> lock(data_mutex_);
    return stop_condition_.wait_for(lock, kReconnectDelay, [this] { return !running_.load(); });
}

void RadioSession::reset_metrics() {
    metric_connection_attempts_.store(0);
    metric_successful_connections_.store(0);
    metric_retry_waits_.store(0);
    metric_read_errors_.store(0);
    metric_settings_updates_.store(0);
    metric_iq_blocks_.store(0);
    metric_iq_bytes_.store(0);
    metric_audio_frames_generated_.store(0);
    metric_audio_frames_written_.store(0);
    metric_audio_frames_dropped_.store(0);
    metric_audio_recoveries_.store(0);
    metric_audio_write_errors_.store(0);
    metric_audio_open_failures_.store(0);
    metric_total_processing_us_.store(0);
    metric_maximum_processing_us_.store(0);
}

void RadioSession::record_processing_time(uint64_t processing_us) {
    metric_total_processing_us_.fetch_add(processing_us);
    uint64_t current = metric_maximum_processing_us_.load();
    while (processing_us > current &&
           !metric_maximum_processing_us_.compare_exchange_weak(current, processing_us)) {
    }
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
