// SPDX-License-Identifier: MIT

#include "rtl_sdr_device.h"

#include <array>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <string>

namespace {

void set_fake_open_result(const char* value) {
#if defined(_WIN32)
    _putenv_s("ZERO_SDR_FAKE_OPEN_RESULT", value ? value : "");
#else
    if (value) setenv("ZERO_SDR_FAKE_OPEN_RESULT", value, 1);
    else unsetenv("ZERO_SDR_FAKE_OPEN_RESULT");
#endif
}

} // namespace

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
    assert(radio.open(1, error) == device::RtlSdrOpenResult::Failed);
    assert(!error.empty());
    set_fake_open_result("-3");
    assert(radio.open(0, error) == device::RtlSdrOpenResult::AccessDenied);
    assert(error.find("access denied") != std::string::npos);
    set_fake_open_result("-6");
    assert(radio.open(0, error) == device::RtlSdrOpenResult::Busy);
    assert(error.find("busy") != std::string::npos);
    set_fake_open_result("-4");
    assert(radio.open(0, error) == device::RtlSdrOpenResult::Disconnected);
    assert(error.find("disconnected") != std::string::npos);
    set_fake_open_result(nullptr);
    assert(radio.open(0, error) == device::RtlSdrOpenResult::Opened);
    assert(radio.is_open());
    const auto gains = radio.tuner_gains(error);
    assert((gains == std::vector<int>{-99, -40, 71, 179, 192}));
    assert(radio.configure(97'400'000, 2'048'000, true, 200, error));
    // The wrapper must snap arbitrary user/config values to a gain reported by
    // the tuner. The fake rejects unsupported values, so 180 can only pass
    // after normalization to 17.9 dB.
    assert(radio.set_gain(false, 180, error));
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
