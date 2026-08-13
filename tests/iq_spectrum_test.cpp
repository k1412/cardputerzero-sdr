// SPDX-License-Identifier: MIT

#include "iq_spectrum.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>

int main() {
    constexpr size_t fft_size = dsp::kSpectrumBinCount;
    constexpr size_t windows = 8;
    constexpr int tone_bin = 20;
    constexpr float pi = 3.14159265358979323846F;
    std::array<uint8_t, fft_size * 2 * windows> iq{};

    for (size_t sample = 0; sample < fft_size * windows; ++sample) {
        const float phase = 2.0F * pi * static_cast<float>(tone_bin) *
                            static_cast<float>(sample) / static_cast<float>(fft_size);
        iq[sample * 2] = static_cast<uint8_t>(std::lround(127.5F + 100.0F * std::cos(phase)));
        iq[sample * 2 + 1] = static_cast<uint8_t>(std::lround(127.5F + 100.0F * std::sin(phase)));
    }

    dsp::IqSpectrum spectrum;
    const auto frame = spectrum.process(iq.data(), iq.size());
    const auto peak = std::max_element(frame.level.begin(), frame.level.end());
    const auto peak_bin = static_cast<size_t>(std::distance(frame.level.begin(), peak));
    assert(frame.sequence == 1);
    assert(peak_bin >= fft_size / 2 + tone_bin - 1);
    assert(peak_bin <= fft_size / 2 + tone_bin + 1);
    assert(*peak >= 80);

    const auto empty = spectrum.process(nullptr, 0);
    assert(empty.sequence == 2);
    assert(std::all_of(empty.level.begin(), empty.level.end(), [](uint8_t level) { return level == 0; }));
}
