// SPDX-License-Identifier: MIT

#pragma once

#include "synthetic_spectrum.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace device {

enum class RadioSessionState : uint8_t {
    Stopped = 0,
    Connecting,
    Live,
    Missing,
    Error,
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
    bool latest_spectrum(dsp::SpectrumFrame& frame) const;
    bool audio_active() const;

private:
    void worker_main();
    bool wait_for_retry();
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
    mutable std::mutex data_mutex_;
    std::condition_variable stop_condition_;
    dsp::SpectrumFrame latest_frame_{};
    std::string status_detail_;
    std::string device_name_;
    std::thread worker_;
};

} // namespace device
