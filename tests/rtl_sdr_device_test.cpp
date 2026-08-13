// SPDX-License-Identifier: MIT

#include "rtl_sdr_device.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>

int main(int argc, char** argv) {
    assert(argc == 2);

    device::RtlSdrDevice unavailable("/path/that/does/not/exist/librtlsdr.so");
    assert(!unavailable.library_available());
    assert(!unavailable.library_error().empty());

    device::RtlSdrDevice radio(argv[1]);
    assert(radio.library_available());
    assert(radio.library_error().empty());

    const auto devices = radio.devices();
    assert(devices.size() == 1);
    assert(devices[0].index == 0);
    assert(devices[0].name == "Fake RTL2832U");
    assert(devices[0].manufacturer == "Zero SDR Tests");
    assert(devices[0].product == "RTL2832U FC0012");
    assert(devices[0].serial == "TEST0001");

    std::string error;
    assert(!radio.open(1, error));
    assert(!error.empty());
    assert(radio.open(0, error));
    assert(radio.is_open());
    assert(radio.configure(97'400'000, 2'048'000, true, 200, error));
    assert(radio.set_gain(false, 190, error));
    assert(radio.set_center_frequency(103'900'000, error));

    std::array<uint8_t, 16'384> samples{};
    size_t bytes_read = 0;
    assert(radio.read_sync(samples.data(), samples.size(), bytes_read, error));
    assert(bytes_read == samples.size());
    assert(samples[0] == 0 && samples[1] == 1 && samples[255] == 255);

    radio.close();
    assert(!radio.is_open());
    assert(!radio.read_sync(samples.data(), samples.size(), bytes_read, error));
}
