// SPDX-License-Identifier: MIT

#include "alsa_audio_sink.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    assert(argc == 2);

    audio::AlsaAudioSink unavailable("/not/a/real/libasound.so");
    assert(!unavailable.library_available());

    audio::AlsaAudioSink sink(argv[1]);
    assert(sink.library_available());
    std::string error;
    assert(sink.open("fake", 32'000, error));
    assert(sink.is_open());

    std::array<int16_t, 512> samples{};
    assert(sink.submit(samples.data(), samples.size()));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (sink.frames_written() < samples.size() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(sink.frames_written() == samples.size());

    sink.set_muted(true);
    assert(sink.muted());
    assert(sink.submit(samples.data(), samples.size()));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(sink.frames_written() == samples.size());
    sink.close();
    assert(!sink.is_open());

    audio::AlsaAudioSink failing_sink(argv[1]);
    assert(failing_sink.open("fail", 32'000, error));
    assert(failing_sink.submit(samples.data(), samples.size()));
    const auto failure_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (failing_sink.is_open() && std::chrono::steady_clock::now() < failure_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    assert(!failing_sink.is_open());
    assert(!failing_sink.submit(samples.data(), samples.size()));
    failing_sink.close();
}
