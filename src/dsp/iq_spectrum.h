// SPDX-License-Identifier: MIT

#pragma once

#include "synthetic_spectrum.h"

#include <cstddef>
#include <cstdint>

namespace dsp {

// Converts unsigned interleaved RTL-SDR IQ bytes into a display-ready frame.
// Up to eight 128-sample Hann-windowed FFTs are averaged per call.
class IqSpectrum {
public:
    SpectrumFrame process(const uint8_t* iq_bytes, size_t byte_count);

private:
    uint64_t sequence_{0};
};

} // namespace dsp
