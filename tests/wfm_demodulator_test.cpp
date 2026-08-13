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
    constexpr float carrier_offset_hz = 12'000.0F;
    constexpr float duration_seconds = 0.1F;
    const size_t sample_count = static_cast<size_t>(dsp::WfmDemodulator::kInputSampleRateHz * duration_seconds);
    std::vector<uint8_t> iq(sample_count * 2);
    float phase = 0.0F;
    for (size_t sample = 0; sample < sample_count; ++sample) {
        const float time = static_cast<float>(sample) /
                           static_cast<float>(dsp::WfmDemodulator::kInputSampleRateHz);
        const float instantaneous_frequency = carrier_offset_hz +
                                              deviation_hz * std::sin(2.0F * pi * audio_frequency_hz * time);
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

    int64_t sample_sum = 0;
    for (auto current = range_begin; current != audio.end(); ++current) sample_sum += *current;
    const auto mean = sample_sum / static_cast<int64_t>(audio.end() - range_begin);
    assert(std::abs(mean) < 400);

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
        const float wanted_frequency = carrier_offset_hz + deviation_hz *
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

    // The 19 kHz FM stereo pilot is above the 16 kHz Nyquist limit of the
    // 32 kHz mono output. Verify that the anti-alias filter removes it instead
    // of folding it down to an audible 13 kHz tone.
    constexpr float pilot_frequency_hz = 19'000.0F;
    constexpr float pilot_alias_hz =
        static_cast<float>(dsp::WfmDemodulator::kAudioSampleRateHz) - pilot_frequency_hz;
    constexpr float pilot_duration_seconds = 0.15F;
    const size_t pilot_sample_count = static_cast<size_t>(
        dsp::WfmDemodulator::kInputSampleRateHz * pilot_duration_seconds);
    std::vector<uint8_t> pilot_iq(pilot_sample_count * 2);
    float pilot_phase = 0.0F;
    for (size_t sample = 0; sample < pilot_sample_count; ++sample) {
        const float time = static_cast<float>(sample) /
                           static_cast<float>(dsp::WfmDemodulator::kInputSampleRateHz);
        const float instantaneous_frequency =
            35'000.0F * std::sin(2.0F * pi * audio_frequency_hz * time) +
            20'000.0F * std::sin(2.0F * pi * pilot_frequency_hz * time);
        pilot_phase += 2.0F * pi * instantaneous_frequency /
                       static_cast<float>(dsp::WfmDemodulator::kInputSampleRateHz);
        pilot_iq[sample * 2] = static_cast<uint8_t>(
            std::lround(127.5F + 100.0F * std::cos(pilot_phase)));
        pilot_iq[sample * 2 + 1] = static_cast<uint8_t>(
            std::lround(127.5F + 100.0F * std::sin(pilot_phase)));
    }

    demodulator.reset();
    const auto pilot_audio = demodulator.process(pilot_iq.data(), pilot_iq.size());
    const size_t pilot_start = dsp::WfmDemodulator::kAudioSampleRateHz / 25;
    const auto tone_magnitude = [&](float frequency_hz) {
        double in_phase = 0.0;
        double quadrature = 0.0;
        for (size_t sample = pilot_start; sample < pilot_audio.size(); ++sample) {
            const double angle = 2.0 * static_cast<double>(pi) *
                                 static_cast<double>(frequency_hz) *
                                 static_cast<double>(sample) /
                                 static_cast<double>(dsp::WfmDemodulator::kAudioSampleRateHz);
            in_phase += static_cast<double>(pilot_audio[sample]) * std::cos(angle);
            quadrature += static_cast<double>(pilot_audio[sample]) * std::sin(angle);
        }
        return std::hypot(in_phase, quadrature);
    };
    const double wanted_magnitude = tone_magnitude(audio_frequency_hz);
    const double aliased_pilot_magnitude = tone_magnitude(pilot_alias_hz);
    assert(wanted_magnitude > 1'000'000.0);
    assert(aliased_pilot_magnitude < wanted_magnitude / 50.0);

    demodulator.reset();
    assert(demodulator.process(nullptr, 0).empty());
}
