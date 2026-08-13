// SPDX-License-Identifier: MIT

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace audio {

class AlsaAudioSink {
public:
    explicit AlsaAudioSink(std::string library_path = {});
    ~AlsaAudioSink();

    AlsaAudioSink(const AlsaAudioSink&) = delete;
    AlsaAudioSink& operator=(const AlsaAudioSink&) = delete;

    bool library_available() const;
    const std::string& library_error() const;
    bool open(const std::string& device_name, uint32_t sample_rate_hz, std::string& error);
    void close();
    bool is_open() const;

    // Copies a chunk into a bounded queue. Returns false when output is closed.
    bool submit(const int16_t* samples, size_t frame_count);
    void set_muted(bool muted);
    bool muted() const;
    uint64_t frames_written() const;
    uint64_t dropped_frames() const;
    uint64_t recoveries() const;
    uint64_t write_errors() const;

private:
    class Impl;
    void worker_main();

    std::unique_ptr<Impl> impl_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::vector<int16_t>> queue_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> healthy_{false};
    std::atomic<bool> muted_{false};
    std::atomic<uint64_t> frames_written_{0};
    std::atomic<uint64_t> dropped_frames_{0};
    std::atomic<uint64_t> recoveries_{0};
    std::atomic<uint64_t> write_errors_{0};
};

} // namespace audio
