// SPDX-License-Identifier: MIT

#include "radio_session.h"

#include <cassert>
#include <chrono>
#include <thread>

int main(int argc, char** argv) {
    assert(argc == 3);
    device::RadioSession session(argv[1], argv[2], "fake");
    session.start();
    assert(session.started());

    dsp::SpectrumFrame frame;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline) {
        if (session.state() == device::RadioSessionState::Live &&
            session.audio_active() && session.latest_spectrum(frame)) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

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
}
