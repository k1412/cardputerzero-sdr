// SPDX-License-Identifier: MIT

#include "iq_spectrum.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace dsp {
namespace {

using Complex = std::complex<float>;
constexpr float kPi = 3.14159265358979323846F;
constexpr size_t kFftSize = kSpectrumBinCount;
constexpr size_t kBytesPerFft = kFftSize * 2;
constexpr size_t kMaximumWindows = 8;

void fft(std::array<Complex, kFftSize>& values) {
    for (size_t index = 1, reversed = 0; index < kFftSize; ++index) {
        size_t bit = kFftSize >> 1U;
        while (reversed & bit) {
            reversed ^= bit;
            bit >>= 1U;
        }
        reversed ^= bit;
        if (index < reversed) std::swap(values[index], values[reversed]);
    }

    for (size_t length = 2; length <= kFftSize; length <<= 1U) {
        const float angle = -2.0F * kPi / static_cast<float>(length);
        const Complex step(std::cos(angle), std::sin(angle));
        for (size_t base = 0; base < kFftSize; base += length) {
            Complex twiddle(1.0F, 0.0F);
            for (size_t offset = 0; offset < length / 2; ++offset) {
                const Complex even = values[base + offset];
                const Complex odd = values[base + offset + length / 2] * twiddle;
                values[base + offset] = even + odd;
                values[base + offset + length / 2] = even - odd;
                twiddle *= step;
            }
        }
    }
}

} // namespace

SpectrumFrame IqSpectrum::process(const uint8_t* iq_bytes, size_t byte_count) {
    SpectrumFrame frame;
    frame.sequence = ++sequence_;
    if (!iq_bytes || byte_count < kBytesPerFft) return frame;

    const size_t available_windows = byte_count / kBytesPerFft;
    const size_t window_count = std::min(available_windows, kMaximumWindows);
    const size_t first_window = available_windows - window_count;
    std::array<float, kFftSize> accumulated_power{};
    std::array<Complex, kFftSize> samples{};

    for (size_t window = 0; window < window_count; ++window) {
        const auto* source = iq_bytes + (first_window + window) * kBytesPerFft;
        for (size_t index = 0; index < kFftSize; ++index) {
            const float hann = 0.5F - 0.5F * std::cos(
                2.0F * kPi * static_cast<float>(index) / static_cast<float>(kFftSize - 1));
            const float i = (static_cast<float>(source[index * 2]) - 127.5F) / 127.5F;
            const float q = (static_cast<float>(source[index * 2 + 1]) - 127.5F) / 127.5F;
            samples[index] = Complex(i * hann, q * hann);
        }
        fft(samples);
        for (size_t bin = 0; bin < kFftSize; ++bin) {
            accumulated_power[bin] += std::norm(samples[bin]);
        }
    }

    constexpr float scale = 1.0F / static_cast<float>(kFftSize * kFftSize);
    for (size_t output_bin = 0; output_bin < kFftSize; ++output_bin) {
        const size_t source_bin = (output_bin + kFftSize / 2) % kFftSize;
        const float power = accumulated_power[source_bin] * scale / static_cast<float>(window_count);
        const float decibels = 10.0F * std::log10(std::max(power, 1.0e-12F));
        const float normalized = (decibels + 75.0F) * (100.0F / 70.0F);
        frame.level[output_bin] = static_cast<uint8_t>(std::clamp(normalized, 0.0F, 100.0F));
    }
    return frame;
}

} // namespace dsp
