// SPDX-License-Identifier: MIT

#include "rtl_sdr_device.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace device {
namespace {

struct rtlsdr_dev;
using rtlsdr_dev_t = rtlsdr_dev;

#if defined(_WIN32)
using LibraryHandle = HMODULE;

LibraryHandle open_library(const char* path) {
    return LoadLibraryA(path);
}

void close_library(LibraryHandle handle) {
    if (handle) FreeLibrary(handle);
}

void* load_symbol(LibraryHandle handle, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(handle, name));
}

std::string loader_error() {
    return "Windows could not load the requested RTL-SDR library";
}
#else
using LibraryHandle = void*;

LibraryHandle open_library(const char* path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void close_library(LibraryHandle handle) {
    if (handle) dlclose(handle);
}

void* load_symbol(LibraryHandle handle, const char* name) {
    return dlsym(handle, name);
}

std::string loader_error() {
    const char* error = dlerror();
    return error ? error : "dynamic loader error";
}
#endif

template <typename Function>
bool resolve(LibraryHandle handle, Function& target, const char* name, std::string& error) {
    target = reinterpret_cast<Function>(load_symbol(handle, name));
    if (!target) {
        error = std::string("librtlsdr is missing symbol ") + name;
        return false;
    }
    return true;
}

std::string safe_text(const char* text) {
    return text ? text : "";
}

} // namespace

class RtlSdrDevice::Impl {
public:
    using GetDeviceCount = uint32_t (*)();
    using GetDeviceName = const char* (*)(uint32_t);
    using GetDeviceUsbStrings = int (*)(uint32_t, char*, char*, char*);
    using Open = int (*)(rtlsdr_dev_t**, uint32_t);
    using Close = int (*)(rtlsdr_dev_t*);
    using SetCenterFreq = int (*)(rtlsdr_dev_t*, uint32_t);
    using SetSampleRate = int (*)(rtlsdr_dev_t*, uint32_t);
    using SetTunerGainMode = int (*)(rtlsdr_dev_t*, int);
    using SetTunerGain = int (*)(rtlsdr_dev_t*, int);
    using GetTunerGains = int (*)(rtlsdr_dev_t*, int*);
    using SetAgcMode = int (*)(rtlsdr_dev_t*, int);
    using ResetBuffer = int (*)(rtlsdr_dev_t*);
    using ReadSync = int (*)(rtlsdr_dev_t*, void*, int, int*);

    explicit Impl(const std::string& requested_path) {
        if (!requested_path.empty()) {
            handle = open_library(requested_path.c_str());
            if (!handle) {
                error = "unable to load " + requested_path + ": " + loader_error();
                return;
            }
        } else {
#if defined(_WIN32)
            constexpr std::array<const char*, 1> candidates = {"rtlsdr.dll"};
#elif defined(__APPLE__)
            constexpr std::array<const char*, 2> candidates = {"librtlsdr.0.dylib", "librtlsdr.dylib"};
#else
            constexpr std::array<const char*, 3> candidates = {
                "/usr/lib/cardputerzero-sdr/librtlsdr.so.0",
                "librtlsdr.so.0",
                "librtlsdr.so",
            };
#endif
            for (const auto* candidate : candidates) {
                handle = open_library(candidate);
                if (handle) break;
            }
            if (!handle) {
                error = "librtlsdr is not installed or is not visible to the dynamic loader";
                return;
            }
        }

        available = resolve(handle, get_device_count, "rtlsdr_get_device_count", error) &&
                    resolve(handle, get_device_name, "rtlsdr_get_device_name", error) &&
                    resolve(handle, get_device_usb_strings, "rtlsdr_get_device_usb_strings", error) &&
                    resolve(handle, open_device, "rtlsdr_open", error) &&
                    resolve(handle, close_device, "rtlsdr_close", error) &&
                    resolve(handle, set_center_freq, "rtlsdr_set_center_freq", error) &&
                    resolve(handle, set_sample_rate, "rtlsdr_set_sample_rate", error) &&
                    resolve(handle, set_tuner_gain_mode, "rtlsdr_set_tuner_gain_mode", error) &&
                    resolve(handle, set_tuner_gain, "rtlsdr_set_tuner_gain", error) &&
                    resolve(handle, get_tuner_gains, "rtlsdr_get_tuner_gains", error) &&
                    resolve(handle, set_agc_mode, "rtlsdr_set_agc_mode", error) &&
                    resolve(handle, reset_buffer_fn, "rtlsdr_reset_buffer", error) &&
                    resolve(handle, read_sync_fn, "rtlsdr_read_sync", error);
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
        if (device && close_device) {
            close_device(device);
            device = nullptr;
        }
    }

    LibraryHandle handle{nullptr};
    rtlsdr_dev_t* device{nullptr};
    std::string error;
    bool available{false};
    GetDeviceCount get_device_count{nullptr};
    GetDeviceName get_device_name{nullptr};
    GetDeviceUsbStrings get_device_usb_strings{nullptr};
    Open open_device{nullptr};
    Close close_device{nullptr};
    SetCenterFreq set_center_freq{nullptr};
    SetSampleRate set_sample_rate{nullptr};
    SetTunerGainMode set_tuner_gain_mode{nullptr};
    SetTunerGain set_tuner_gain{nullptr};
    GetTunerGains get_tuner_gains{nullptr};
    SetAgcMode set_agc_mode{nullptr};
    ResetBuffer reset_buffer_fn{nullptr};
    ReadSync read_sync_fn{nullptr};
};

RtlSdrDevice::RtlSdrDevice(std::string library_path)
    : impl_(std::make_unique<Impl>(library_path)) {}

RtlSdrDevice::~RtlSdrDevice() = default;

bool RtlSdrDevice::library_available() const {
    return impl_->available;
}

const std::string& RtlSdrDevice::library_error() const {
    return impl_->error;
}

std::vector<RtlSdrInfo> RtlSdrDevice::devices() const {
    std::vector<RtlSdrInfo> result;
    if (!impl_->available) return result;

    const uint32_t count = impl_->get_device_count();
    result.reserve(count);
    for (uint32_t index = 0; index < count; ++index) {
        std::array<char, 256> manufacturer{};
        std::array<char, 256> product{};
        std::array<char, 256> serial{};
        if (impl_->get_device_usb_strings(index,
                                          manufacturer.data(),
                                          product.data(),
                                          serial.data()) != 0) {
            manufacturer[0] = product[0] = serial[0] = '\0';
        }
        result.push_back(RtlSdrInfo{
            index,
            safe_text(impl_->get_device_name(index)),
            manufacturer.data(),
            product.data(),
            serial.data(),
        });
    }
    return result;
}

bool RtlSdrDevice::open(uint32_t index, std::string& error) {
    close();
    if (!impl_->available) {
        error = impl_->error;
        return false;
    }
    if (index >= impl_->get_device_count()) {
        error = "RTL-SDR device index is out of range";
        return false;
    }
    if (impl_->open_device(&impl_->device, index) != 0 || !impl_->device) {
        impl_->device = nullptr;
        error = "failed to open RTL-SDR device";
        return false;
    }
    error.clear();
    return true;
}

void RtlSdrDevice::close() {
    impl_->close();
}

bool RtlSdrDevice::is_open() const {
    return impl_->device != nullptr;
}

std::vector<int> RtlSdrDevice::tuner_gains(std::string& error) const {
    if (!is_open()) {
        error = "RTL-SDR device is not open";
        return {};
    }
    const int count = impl_->get_tuner_gains(impl_->device, nullptr);
    if (count <= 0 || count > 256) {
        error = "RTL-SDR returned an invalid tuner gain count";
        return {};
    }
    std::vector<int> gains(static_cast<size_t>(count));
    const int returned = impl_->get_tuner_gains(impl_->device, gains.data());
    if (returned <= 0 || returned > count) {
        error = "RTL-SDR failed to return tuner gain values";
        return {};
    }
    gains.resize(static_cast<size_t>(returned));
    std::sort(gains.begin(), gains.end());
    gains.erase(std::unique(gains.begin(), gains.end()), gains.end());
    error.clear();
    return gains;
}

bool RtlSdrDevice::configure(uint32_t center_frequency_hz,
                             uint32_t sample_rate_hz,
                             bool automatic_gain,
                             int gain_tenths_db,
                             std::string& error) {
    if (!is_open()) {
        error = "RTL-SDR device is not open";
        return false;
    }
    if (sample_rate_hz == 0 || impl_->set_sample_rate(impl_->device, sample_rate_hz) != 0) {
        error = "failed to set RTL-SDR sample rate";
        return false;
    }
    if (!set_center_frequency(center_frequency_hz, error) ||
        !set_gain(automatic_gain, gain_tenths_db, error) ||
        !reset_buffer(error)) {
        return false;
    }
    error.clear();
    return true;
}

bool RtlSdrDevice::set_center_frequency(uint32_t center_frequency_hz, std::string& error) {
    if (!is_open() || center_frequency_hz == 0 ||
        impl_->set_center_freq(impl_->device, center_frequency_hz) != 0) {
        error = "failed to set RTL-SDR center frequency";
        return false;
    }
    error.clear();
    return true;
}

bool RtlSdrDevice::set_gain(bool automatic_gain, int gain_tenths_db, std::string& error) {
    if (!is_open()) {
        error = "RTL-SDR device is not open";
        return false;
    }
    if (impl_->set_agc_mode(impl_->device, 0) != 0 ||
        impl_->set_tuner_gain_mode(impl_->device, automatic_gain ? 0 : 1) != 0) {
        error = "failed to set RTL-SDR gain mode";
        return false;
    }
    if (!automatic_gain) {
        const auto gains = tuner_gains(error);
        if (gains.empty()) return false;
        const auto nearest = std::min_element(
            gains.begin(), gains.end(), [gain_tenths_db](int left, int right) {
                const auto left_distance = std::llabs(
                    static_cast<long long>(left) - static_cast<long long>(gain_tenths_db));
                const auto right_distance = std::llabs(
                    static_cast<long long>(right) - static_cast<long long>(gain_tenths_db));
                return left_distance < right_distance;
            });
        if (impl_->set_tuner_gain(impl_->device, *nearest) != 0) {
            error = "failed to set RTL-SDR tuner gain";
            return false;
        }
    }
    error.clear();
    return true;
}

bool RtlSdrDevice::reset_buffer(std::string& error) {
    if (!is_open() || impl_->reset_buffer_fn(impl_->device) != 0) {
        error = "failed to reset RTL-SDR sample buffer";
        return false;
    }
    error.clear();
    return true;
}

bool RtlSdrDevice::read_sync(uint8_t* buffer,
                             size_t capacity,
                             size_t& bytes_read,
                             std::string& error) {
    bytes_read = 0;
    if (!is_open() || !buffer || capacity == 0 || capacity > static_cast<size_t>(INT_MAX)) {
        error = "invalid RTL-SDR read request";
        return false;
    }
    int count = 0;
    const int result = impl_->read_sync_fn(impl_->device,
                                           buffer,
                                           static_cast<int>(capacity),
                                           &count);
    if (result != 0 || count < 0 || static_cast<size_t>(count) > capacity) {
        error = "RTL-SDR synchronous read failed";
        return false;
    }
    bytes_read = static_cast<size_t>(count);
    error.clear();
    return true;
}

} // namespace device
