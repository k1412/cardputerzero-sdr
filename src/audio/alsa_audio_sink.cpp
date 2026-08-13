// SPDX-License-Identifier: MIT

#include "alsa_audio_sink.h"

#include <array>
#include <climits>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace audio {
namespace {

struct snd_pcm;
using snd_pcm_t = snd_pcm;

constexpr int kPlaybackStream = 0;
constexpr int kAccessRwInterleaved = 3;
constexpr int kFormatS16Le = 2;
constexpr unsigned int kLatencyMicroseconds = 100'000;
constexpr size_t kMaximumQueuedFrames = 4'096;

#if defined(_WIN32)
using LibraryHandle = HMODULE;
LibraryHandle open_library(const char* path) { return LoadLibraryA(path); }
void close_library(LibraryHandle handle) { if (handle) FreeLibrary(handle); }
void* load_symbol(LibraryHandle handle, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(handle, name));
}
std::string loader_error() { return "Windows could not load the requested ALSA compatibility library"; }
#else
using LibraryHandle = void*;
LibraryHandle open_library(const char* path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
void close_library(LibraryHandle handle) { if (handle) dlclose(handle); }
void* load_symbol(LibraryHandle handle, const char* name) { return dlsym(handle, name); }
std::string loader_error() {
    const char* error = dlerror();
    return error ? error : "dynamic loader error";
}
#endif

template <typename Function>
bool resolve(LibraryHandle handle, Function& target, const char* name, std::string& error) {
    target = reinterpret_cast<Function>(load_symbol(handle, name));
    if (!target) {
        error = std::string("libasound is missing symbol ") + name;
        return false;
    }
    return true;
}

} // namespace

class AlsaAudioSink::Impl {
public:
    using Open = int (*)(snd_pcm_t**, const char*, int, int);
    using Close = int (*)(snd_pcm_t*);
    using SetParams = int (*)(snd_pcm_t*, int, int, unsigned int, unsigned int, int, unsigned int);
    using WriteInterleaved = long (*)(snd_pcm_t*, const void*, unsigned long);
    using Recover = int (*)(snd_pcm_t*, int, int);
    using Drop = int (*)(snd_pcm_t*);
    using ErrorText = const char* (*)(int);

    explicit Impl(const std::string& requested_path) {
        if (!requested_path.empty()) {
            handle = open_library(requested_path.c_str());
            if (!handle) {
                error = "unable to load " + requested_path + ": " + loader_error();
                return;
            }
        } else {
#if defined(_WIN32)
            constexpr std::array<const char*, 1> candidates = {"asound.dll"};
#elif defined(__APPLE__)
            constexpr std::array<const char*, 1> candidates = {"libasound.dylib"};
#else
            constexpr std::array<const char*, 2> candidates = {"libasound.so.2", "libasound.so"};
#endif
            for (const auto* candidate : candidates) {
                handle = open_library(candidate);
                if (handle) break;
            }
            if (!handle) {
                error = "libasound is not installed or is not visible to the dynamic loader";
                return;
            }
        }

        available = resolve(handle, open_pcm, "snd_pcm_open", error) &&
                    resolve(handle, close_pcm, "snd_pcm_close", error) &&
                    resolve(handle, set_params, "snd_pcm_set_params", error) &&
                    resolve(handle, write_interleaved, "snd_pcm_writei", error) &&
                    resolve(handle, recover, "snd_pcm_recover", error) &&
                    resolve(handle, drop, "snd_pcm_drop", error) &&
                    resolve(handle, error_text, "snd_strerror", error);
        if (!available) {
            close_library(handle);
            handle = nullptr;
        }
    }

    ~Impl() {
        close();
        close_library(handle);
    }

    void close() {
        if (pcm && close_pcm) {
            drop(pcm);
            close_pcm(pcm);
            pcm = nullptr;
        }
    }

    std::string describe(int code) const {
        const char* text = error_text ? error_text(code) : nullptr;
        return text ? text : "unknown ALSA error";
    }

    LibraryHandle handle{nullptr};
    snd_pcm_t* pcm{nullptr};
    std::string error;
    bool available{false};
    Open open_pcm{nullptr};
    Close close_pcm{nullptr};
    SetParams set_params{nullptr};
    WriteInterleaved write_interleaved{nullptr};
    Recover recover{nullptr};
    Drop drop{nullptr};
    ErrorText error_text{nullptr};
};

AlsaAudioSink::AlsaAudioSink(std::string library_path)
    : impl_(std::make_unique<Impl>(library_path)) {}

AlsaAudioSink::~AlsaAudioSink() {
    close();
}

bool AlsaAudioSink::library_available() const { return impl_->available; }
const std::string& AlsaAudioSink::library_error() const { return impl_->error; }

bool AlsaAudioSink::open(const std::string& device_name,
                         uint32_t sample_rate_hz,
                         std::string& error) {
    close();
    if (!impl_->available) {
        error = impl_->error;
        return false;
    }
    const std::string selected_device = device_name.empty() ? "default" : device_name;
    int result = impl_->open_pcm(&impl_->pcm, selected_device.c_str(), kPlaybackStream, 0);
    if (result < 0 || !impl_->pcm) {
        impl_->pcm = nullptr;
        error = "failed to open ALSA device " + selected_device + ": " + impl_->describe(result);
        return false;
    }
    result = impl_->set_params(impl_->pcm,
                               kFormatS16Le,
                               kAccessRwInterleaved,
                               1,
                               sample_rate_hz,
                               1,
                               kLatencyMicroseconds);
    if (result < 0) {
        error = "failed to configure ALSA mono PCM: " + impl_->describe(result);
        impl_->close();
        return false;
    }

    frames_written_.store(0);
    dropped_frames_.store(0);
    running_.store(true);
    worker_ = std::thread(&AlsaAudioSink::worker_main, this);
    error.clear();
    return true;
}

void AlsaAudioSink::close() {
    if (running_.exchange(false)) {
        condition_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }
    impl_->close();
}

bool AlsaAudioSink::is_open() const { return impl_->pcm != nullptr && running_.load(); }

bool AlsaAudioSink::submit(const int16_t* samples, size_t frame_count) {
    if (!is_open() || !samples || frame_count == 0) return false;
    if (muted_.load()) return true;

    std::lock_guard<std::mutex> lock(mutex_);
    size_t queued_frames = 0;
    for (const auto& chunk : queue_) queued_frames += chunk.size();
    while (!queue_.empty() && queued_frames + frame_count > kMaximumQueuedFrames) {
        dropped_frames_.fetch_add(queue_.front().size());
        queued_frames -= queue_.front().size();
        queue_.pop_front();
    }
    if (frame_count > kMaximumQueuedFrames) {
        const size_t skipped = frame_count - kMaximumQueuedFrames;
        dropped_frames_.fetch_add(skipped);
        samples += skipped;
        frame_count = kMaximumQueuedFrames;
    }
    queue_.emplace_back(samples, samples + frame_count);
    condition_.notify_one();
    return true;
}

void AlsaAudioSink::set_muted(bool muted) {
    muted_.store(muted);
    if (muted) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }
}

bool AlsaAudioSink::muted() const { return muted_.load(); }
uint64_t AlsaAudioSink::frames_written() const { return frames_written_.load(); }
uint64_t AlsaAudioSink::dropped_frames() const { return dropped_frames_.load(); }

void AlsaAudioSink::worker_main() {
    while (running_.load()) {
        std::vector<int16_t> chunk;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return !running_.load() || !queue_.empty(); });
            if (!running_.load()) break;
            chunk = std::move(queue_.front());
            queue_.pop_front();
        }
        if (muted_.load()) continue;

        size_t offset = 0;
        while (running_.load() && offset < chunk.size()) {
            const auto remaining = static_cast<unsigned long>(chunk.size() - offset);
            long written = impl_->write_interleaved(impl_->pcm, chunk.data() + offset, remaining);
            if (written < 0) {
                const int recovered = impl_->recover(impl_->pcm, static_cast<int>(written), 1);
                if (recovered < 0) break;
                continue;
            }
            if (written == 0) break;
            offset += static_cast<size_t>(written);
            frames_written_.fetch_add(static_cast<uint64_t>(written));
        }
    }
}

} // namespace audio
