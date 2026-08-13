// SPDX-License-Identifier: MIT

#include "radio_session.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

void set_fake_open_result(const char* value) {
#if defined(_WIN32)
    _putenv_s("ZERO_SDR_FAKE_OPEN_RESULT", value ? value : "");
#else
    if (value) setenv("ZERO_SDR_FAKE_OPEN_RESULT", value, 1);
    else unsetenv("ZERO_SDR_FAKE_OPEN_RESULT");
#endif
}

bool wait_for_state(device::RadioSession& session, device::RadioSessionState expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline) {
        if (session.state() == expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

bool wait_for_live_frame(device::RadioSession& session,
                         dsp::SpectrumFrame& frame,
                         bool expected_audio_active) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (session.state() == device::RadioSessionState::Live &&
            session.audio_active() == expected_audio_active &&
            session.latest_spectrum(frame)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    assert(argc == 3);
    device::RadioSession session(argv[1], argv[2], "fake");
    session.start();
    assert(session.started());

    dsp::SpectrumFrame frame;
    assert(wait_for_live_frame(session, frame, true));
    assert(session.state() == device::RadioSessionState::Live);
    assert(session.device_name() == "Fake RTL2832U");
    assert((session.supported_gains() == std::vector<int>{-99, -40, 71, 179, 192}));
    assert(frame.sequence > 0);
    assert(session.audio_active());
    auto metrics = session.metrics();
    assert(metrics.connection_attempts == 1);
    assert(metrics.successful_connections == 1);
    assert(metrics.iq_blocks > 0);
    assert(metrics.iq_bytes >= metrics.iq_blocks * 16'384);
    assert(metrics.audio_frames_generated > 0);
    assert(metrics.total_processing_us > 0);
    assert(metrics.maximum_processing_us > 0);
    const auto audio_metrics_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (session.metrics().audio_frames_written == 0 &&
           std::chrono::steady_clock::now() < audio_metrics_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    metrics = session.metrics();
    assert(metrics.audio_frames_written > 0);
    assert(metrics.audio_frames_dropped == 0);
    assert(metrics.audio_recoveries == 0);
    assert(metrics.audio_write_errors == 0);
    assert(metrics.audio_open_failures == 0);
    session.request_frequency(103'900'000);
    session.request_gain(false, 190);
    session.request_muted(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(session.state() == device::RadioSessionState::Live);
    assert(session.metrics().settings_updates >= 1);

    session.stop();
    assert(!session.started());
    assert(session.state() == device::RadioSessionState::Stopped);
    assert(!session.audio_active());

    set_fake_open_result("-3");
    device::RadioSession access_denied_session(argv[1], argv[2], "fake");
    access_denied_session.start();
    assert(wait_for_state(access_denied_session, device::RadioSessionState::AccessDenied));
    assert(access_denied_session.status_detail().find("access denied") != std::string::npos);
    assert(access_denied_session.metrics().successful_connections == 0);
    access_denied_session.stop();

    set_fake_open_result("-6");
    device::RadioSession busy_session(argv[1], argv[2], "fake");
    busy_session.start();
    assert(wait_for_state(busy_session, device::RadioSessionState::Busy));
    assert(busy_session.status_detail().find("busy") != std::string::npos);
    assert(busy_session.metrics().successful_connections == 0);
    busy_session.stop();
    set_fake_open_result(nullptr);

    // Missing ALSA must not prevent live RF and spectrum operation.
    device::RadioSession no_audio_session(argv[1], "/not/a/real/libasound.so", "fake");
    no_audio_session.start();
    assert(wait_for_live_frame(no_audio_session, frame, false));
    assert(!no_audio_session.status_detail().empty());
    metrics = no_audio_session.metrics();
    assert(metrics.audio_open_failures == 1);
    assert(metrics.audio_frames_generated == 0);
    no_audio_session.stop();

    // A permanent playback error must make audio inactive without taking the
    // receiver offline or leaving a joinable audio worker behind.
    device::RadioSession failing_audio_session(argv[1], argv[2], "fail");
    failing_audio_session.start();
    assert(wait_for_live_frame(failing_audio_session, frame, false));
    assert(failing_audio_session.status_detail().find("unrecoverable") != std::string::npos);
    const auto metrics_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (failing_audio_session.metrics().audio_write_errors == 0 &&
           std::chrono::steady_clock::now() < metrics_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    metrics = failing_audio_session.metrics();
    assert(metrics.audio_frames_generated > 0);
    assert(metrics.audio_write_errors == 1);
    assert(metrics.audio_recoveries == 0);
    failing_audio_session.stop();
}
