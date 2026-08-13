// SPDX-License-Identifier: MIT

#pragma once

#include "synthetic_spectrum.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace device {

enum class RadioSessionState : uint8_t {
    Stopped = 0,
    Connecting,
    Live,
    Missing,
    Error,
};

struct RadioSessionMetrics {
    uint64_t connection_attempts{0};
    uint64_t successful_connections{0};
    uint64_t retry_waits{0};
    uint64_t read_errors{0};
    uint64_t settings_updates{0};
    uint64_t iq_blocks{0};
    uint64_t iq_bytes{0};
    uint64_t audio_frames_generated{0};
    uint64_t audio_frames_written{0};
    uint64_t audio_frames_dropped{0};
    uint64_t audio_recoveries{0};
    uint64_t audio_write_errors{0};
    uint64_t audio_open_failures{0};
    uint64_t total_processing_us{0};
    uint64_t maximum_processing_us{0};
};

class RadioSession {
public:
    explicit RadioSession(std::string library_path = {},
                          std::string audio_library_path = {},
                          std::string audio_device_name = {});
    ~RadioSession();

    RadioSession(const RadioSession&) = delete;
    RadioSession& operator=(const RadioSession&) = delete;

    void start();
    void stop();
    bool started() const;

    void request_frequency(uint32_t frequency_hz);
    void request_gain(bool automatic_gain, int gain_tenths_db);
    void request_muted(bool muted);

    RadioSessionState state() const;
    std::string status_detail() const;
    std::string device_name() const;
    std::vector<int> supported_gains() const;
    bool latest_spectrum(dsp::SpectrumFrame& frame) const;
    bool audio_active() const;
    RadioSessionMetrics metrics() const;

private:
    void worker_main();
    bool wait_for_retry();
    void reset_metrics();
    void record_processing_time(uint64_t processing_us);
    void set_state(RadioSessionState state, std::string detail = {}, std::string device_name = {});

    std::string library_path_;
    std::string audio_library_path_;
    std::string audio_device_name_;
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> requested_frequency_hz_{97'400'000};
    std::atomic<bool> requested_automatic_gain_{true};
    std::atomic<int> requested_gain_tenths_db_{200};
    std::atomic<bool> requested_muted_{false};
    std::atomic<uint64_t> settings_generation_{1};
    std::atomic<RadioSessionState> state_{RadioSessionState::Stopped};
    std::atomic<bool> audio_active_{false};
    std::atomic<uint64_t> metric_connection_attempts_{0};
    std::atomic<uint64_t> metric_successful_connections_{0};
    std::atomic<uint64_t> metric_retry_waits_{0};
    std::atomic<uint64_t> metric_read_errors_{0};
    std::atomic<uint64_t> metric_settings_updates_{0};
    std::atomic<uint64_t> metric_iq_blocks_{0};
    std::atomic<uint64_t> metric_iq_bytes_{0};
    std::atomic<uint64_t> metric_audio_frames_generated_{0};
    std::atomic<uint64_t> metric_audio_frames_written_{0};
    std::atomic<uint64_t> metric_audio_frames_dropped_{0};
    std::atomic<uint64_t> metric_audio_recoveries_{0};
    std::atomic<uint64_t> metric_audio_write_errors_{0};
    std::atomic<uint64_t> metric_audio_open_failures_{0};
    std::atomic<uint64_t> metric_total_processing_us_{0};
    std::atomic<uint64_t> metric_maximum_processing_us_{0};
    mutable std::mutex data_mutex_;
    std::condition_variable stop_condition_;
    dsp::SpectrumFrame latest_frame_{};
    std::string status_detail_;
    std::string device_name_;
    std::vector<int> supported_gains_;
    std::thread worker_;
};

} // namespace device
