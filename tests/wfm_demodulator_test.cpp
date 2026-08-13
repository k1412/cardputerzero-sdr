// SPDX-License-Identifier: MIT

#include "wfm_demodulator.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

int main() {
    constexpr float pi = 3.14159265358979323846F;
    constexpr float audio_frequency_hz = 1'000.0F;
    constexpr float deviation_hz = 50'000.0F;
    constexpr float duration_seconds = 0.1F;
    const size_t sample_count = static_cast<size_t>(dsp::WfmDemodulator::kInputSampleRateHz * duration_seconds);
    std::vector<uint8_t> iq(sample_count * 2);
    float phase = 0.0F;
    for (size_t sample = 0; sample < sample_count; ++sample) {
        const float time = static_cast<float>(sample) /
                           static_cast<float>(dsp::WfmDemodulator::kInputSampleRateHz);
        const float instantaneous_frequency = deviation_hz * std::sin(2.0F * pi * audio_frequency_hz * time);
        phase += 2.0F * pi * instantaneous_frequency /
                 static_cast<float>(dsp::WfmDemodulator::kInputSampleRateHz);
        iq[sample * 2] = static_cast<uint8_t>(std::lround(127.5F + 100.0F * std::cos(phase)));
        iq[sample * 2 + 1] = static_cast<uint8_t>(std::lround(127.5F + 100.0F * std::sin(phase)));
    }

    dsp::WfmDemodulator demodulator;
    const auto audio = demodulator.process(iq.data(), iq.size());
    assert(audio.size() == static_cast<size_t>(dsp::WfmDemodulator::kAudioSampleRateHz * duration_seconds));

    // Ignore de-emphasis startup and verify the recovered waveform is present,
    // bipolar, and close to the expected 1 kHz zero-crossing count.
    const size_t start = dsp::WfmDemodulator::kAudioSampleRateHz / 100;
    const auto range_begin = audio.begin() + static_cast<std::ptrdiff_t>(start);
    const auto [minimum, maximum] = std::minmax_element(range_begin, audio.end());
    assert(*minimum < -5'000);
    assert(*maximum > 5'000);

    size_t positive_crossings = 0;
    for (auto current = range_begin + 1; current != audio.end(); ++current) {
        if (*(current - 1) <= 0 && *current > 0) ++positive_crossings;
    }
    assert(positive_crossings >= 85 && positive_crossings <= 95);

    // A strong carrier well outside the selected 200 kHz FM channel should
    // not destroy recovery of the centered station. This specifically guards
    // the complex channel filter that runs before the discriminator.
    std::vector<uint8_t> crowded_iq(sample_count * 2);
    float wanted_phase = 0.0F;
    float adjacent_phase = 0.0F;
    for (size_t sample = 0; sample < sample_count; ++sample) {
        const float time = static_cast<float>(sample) /
                           static_cast<float>(dsp::WfmDemodulator::kInputSampleRateHz);
        const float wanted_frequency = deviation_hz *
                                       std::sin(2.0F * pi * audio_frequency_hz * time);
        wanted_phase += 2.0F * pi * wanted_frequency /
                        static_cast<float>(dsp::WfmDemodulator::kInputSampleRateHz);
        adjacent_phase += 2.0F * pi * 350'000.0F /
                          static_cast<float>(dsp::WfmDemodulator::kInputSampleRateHz);
        const float i = 70.0F * std::cos(wanted_phase) + 45.0F * std::cos(adjacent_phase);
        const float q = 70.0F * std::sin(wanted_phase) + 45.0F * std::sin(adjacent_phase);
        crowded_iq[sample * 2] = static_cast<uint8_t>(std::lround(127.5F + i));
        crowded_iq[sample * 2 + 1] = static_cast<uint8_t>(std::lround(127.5F + q));
    }

    demodulator.reset();
    const auto crowded_audio = demodulator.process(crowded_iq.data(), crowded_iq.size());
    assert(crowded_audio.size() == audio.size());
    size_t crowded_positive_crossings = 0;
    for (auto current = crowded_audio.begin() + static_cast<std::ptrdiff_t>(start + 1);
         current != crowded_audio.end();
         ++current) {
        if (*(current - 1) <= 0 && *current > 0) ++crowded_positive_crossings;
    }
    assert(crowded_positive_crossings >= 85 && crowded_positive_crossings <= 95);

    demodulator.reset();
    assert(demodulator.process(nullptr, 0).empty());
}
