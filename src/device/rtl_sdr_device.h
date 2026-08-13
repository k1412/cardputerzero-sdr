// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace device {

struct RtlSdrInfo {
    uint32_t index{0};
    std::string name;
    std::string manufacturer;
    std::string product;
    std::string serial;
};

class RtlSdrDevice {
public:
    // An empty path searches the platform's standard librtlsdr names.
    explicit RtlSdrDevice(std::string library_path = {});
    ~RtlSdrDevice();

    RtlSdrDevice(const RtlSdrDevice&) = delete;
    RtlSdrDevice& operator=(const RtlSdrDevice&) = delete;

    bool library_available() const;
    const std::string& library_error() const;
    std::vector<RtlSdrInfo> devices() const;

    bool open(uint32_t index, std::string& error);
    void close();
    bool is_open() const;
    std::vector<int> tuner_gains(std::string& error) const;

    bool configure(uint32_t center_frequency_hz,
                   uint32_t sample_rate_hz,
                   bool automatic_gain,
                   int gain_tenths_db,
                   std::string& error);
    bool set_center_frequency(uint32_t center_frequency_hz, std::string& error);
    bool set_gain(bool automatic_gain, int gain_tenths_db, std::string& error);
    bool reset_buffer(std::string& error);
    bool read_sync(uint8_t* buffer, size_t capacity, size_t& bytes_read, std::string& error);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace device
