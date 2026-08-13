// SPDX-License-Identifier: MIT

#include "synthetic_spectrum.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace dsp {
namespace {

constexpr std::array<uint32_t, 5> kDemoStationsHz = {
    90'500'000,
    94'500'000,
    97'400'000,
    103'900'000,
    106'100'000,
};

float gaussian(float x, float width) {
    const float normalized = x / width;
    return std::exp(-0.5F * normalized * normalized);
}

} // namespace

SpectrumFrame SyntheticSpectrum::next(uint32_t center_frequency_hz) {
    SpectrumFrame frame;
    frame.sequence = ++sequence_;

    constexpr float visible_span_hz = 2'400'000.0F;
    constexpr float bin_width_hz = visible_span_hz / static_cast<float>(kSpectrumBinCount);
    const float sweep = std::sin(static_cast<float>(sequence_) * 0.085F);

    for (size_t bin = 0; bin < frame.level.size(); ++bin) {
        noise_state_ = noise_state_ * 1664525U + 1013904223U;
        const float noise = static_cast<float>((noise_state_ >> 24U) & 0x0fU);
        const float offset_hz = (static_cast<float>(bin) - static_cast<float>(kSpectrumBinCount - 1) / 2.0F)
                              * bin_width_hz;
        const float absolute_hz = static_cast<float>(center_frequency_hz) + offset_hz;

        float level = 13.0F + noise + 2.0F * std::sin(static_cast<float>(bin) * 0.31F + sweep);
        for (size_t station = 0; station < kDemoStationsHz.size(); ++station) {
            const float separation = absolute_hz - static_cast<float>(kDemoStationsHz[station]);
            const float strength = station == 2 ? 76.0F : 52.0F + static_cast<float>(station) * 4.0F;
            level += strength * gaussian(separation, 72'000.0F);
        }

        frame.level[bin] = static_cast<uint8_t>(std::clamp(level, 0.0F, 100.0F));
    }

    return frame;
}

} // namespace dsp
