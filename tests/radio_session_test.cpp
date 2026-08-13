// SPDX-License-Identifier: MIT

#include "radio_session.h"

#include <cassert>
#include <chrono>
#include <string>
#include <thread>

namespace {

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
    assert(frame.sequence > 0);
    assert(session.audio_active());
    session.request_frequency(103'900'000);
    session.request_gain(false, 190);
    session.request_muted(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    assert(session.state() == device::RadioSessionState::Live);

    session.stop();
    assert(!session.started());
    assert(session.state() == device::RadioSessionState::Stopped);
    assert(!session.audio_active());

    // Missing ALSA must not prevent live RF and spectrum operation.
    device::RadioSession no_audio_session(argv[1], "/not/a/real/libasound.so", "fake");
    no_audio_session.start();
    assert(wait_for_live_frame(no_audio_session, frame, false));
    assert(!no_audio_session.status_detail().empty());
    no_audio_session.stop();

    // A permanent playback error must make audio inactive without taking the
    // receiver offline or leaving a joinable audio worker behind.
    device::RadioSession failing_audio_session(argv[1], argv[2], "fail");
    failing_audio_session.start();
    assert(wait_for_live_frame(failing_audio_session, frame, false));
    assert(failing_audio_session.status_detail().find("unrecoverable") != std::string::npos);
    failing_audio_session.stop();
}
