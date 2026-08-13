// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsp {

constexpr size_t kSpectrumBinCount = 128;

struct SpectrumFrame {
    std::array<uint8_t, kSpectrumBinCount> level{};
    uint64_t sequence{0};
};

class SyntheticSpectrum {
public:
    SpectrumFrame next(uint32_t center_frequency_hz);

private:
    uint32_t noise_state_{0x72a14f3dU};
    uint64_t sequence_{0};
};

} // namespace dsp
