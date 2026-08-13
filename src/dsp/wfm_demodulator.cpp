// SPDX-License-Identifier: MIT

#include "wfm_demodulator.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace dsp {
namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kMaximumDeviationHz = 75'000.0F;
constexpr float kMaximumPhaseStep = 2.0F * kPi * kMaximumDeviationHz /
                                    static_cast<float>(WfmDemodulator::kDemodSampleRateHz);
constexpr float kRfCutoffHz = 100'000.0F;
// 50 us is the FM broadcast de-emphasis used in Europe, China, and many other
// regions. The audio setting can expose 75 us later for North America.
constexpr float kDeemphasisSeconds = 50.0e-6F;
constexpr float kAudioPeriodSeconds = 1.0F / static_cast<float>(WfmDemodulator::kAudioSampleRateHz);
constexpr float kDeemphasisAlpha = kAudioPeriodSeconds / (kDeemphasisSeconds + kAudioPeriodSeconds);
constexpr float kDcBlockCutoffHz = 30.0F;
const float kDcBlockAlpha = 1.0F - std::exp(
    -2.0F * kPi * kDcBlockCutoffHz / static_cast<float>(WfmDemodulator::kAudioSampleRateHz));

const std::array<float, 33>& rf_filter() {
    static const auto coefficients = [] {
        std::array<float, 33> result{};
        constexpr int midpoint = static_cast<int>(result.size() / 2);
        constexpr float normalized_cutoff = kRfCutoffHz /
                                            static_cast<float>(WfmDemodulator::kInputSampleRateHz);
        float sum = 0.0F;
        for (size_t index = 0; index < result.size(); ++index) {
            const int offset = static_cast<int>(index) - midpoint;
            const float ideal = offset == 0
                ? 2.0F * normalized_cutoff
                : std::sin(2.0F * kPi * normalized_cutoff * static_cast<float>(offset)) /
                      (kPi * static_cast<float>(offset));
            const float hamming = 0.54F - 0.46F * std::cos(
                2.0F * kPi * static_cast<float>(index) /
                static_cast<float>(result.size() - 1));
            result[index] = ideal * hamming;
            sum += result[index];
        }
        for (auto& coefficient : result) coefficient /= sum;
        return result;
    }();
    return coefficients;
}

} // namespace

std::vector<int16_t> WfmDemodulator::process(const uint8_t* iq_bytes, size_t byte_count) {
    std::vector<int16_t> audio;
    if (!iq_bytes || byte_count < 2) return audio;
    audio.reserve(byte_count / 2 / (kRfDecimation * kAudioDecimation) + 1);

    const size_t complex_samples = byte_count / 2;
    for (size_t index = 0; index < complex_samples; ++index) {
        i_history_[history_position_] =
            (static_cast<float>(iq_bytes[index * 2]) - 127.5F) / 127.5F;
        q_history_[history_position_] =
            (static_cast<float>(iq_bytes[index * 2 + 1]) - 127.5F) / 127.5F;
        history_position_ = (history_position_ + 1) % kRfFilterTaps;

        ++rf_decimation_phase_;
        if (rf_decimation_phase_ < kRfDecimation) continue;
        rf_decimation_phase_ = 0;

        float current_i = 0.0F;
        float current_q = 0.0F;
        size_t history_index = history_position_;
        const auto& coefficients = rf_filter();
        for (size_t tap = 0; tap < kRfFilterTaps; ++tap) {
            history_index = history_index == 0 ? kRfFilterTaps - 1 : history_index - 1;
            current_i += i_history_[history_index] * coefficients[tap];
            current_q += q_history_[history_index] * coefficients[tap];
        }

        if (has_previous_) {
            const float dot = previous_i_ * current_i + previous_q_ * current_q;
            const float cross = previous_i_ * current_q - previous_q_ * current_i;
            discriminator_sum_ += std::atan2(cross, dot);
        } else {
            has_previous_ = true;
        }
        previous_i_ = current_i;
        previous_q_ = current_q;

        ++audio_decimation_phase_;
        if (audio_decimation_phase_ < kAudioDecimation) continue;
        audio_decimation_phase_ = 0;

        const float average_phase = discriminator_sum_ / static_cast<float>(kAudioDecimation);
        discriminator_sum_ = 0.0F;
        const float normalized = std::clamp(average_phase / kMaximumPhaseStep, -1.0F, 1.0F);
        deemphasis_state_ += kDeemphasisAlpha * (normalized - deemphasis_state_);
        dc_estimate_ += kDcBlockAlpha * (deemphasis_state_ - dc_estimate_);
        const float dc_blocked = deemphasis_state_ - dc_estimate_;
        const float scaled = std::clamp(dc_blocked * 29'000.0F, -32'768.0F, 32'767.0F);
        audio.push_back(static_cast<int16_t>(std::lround(scaled)));
    }
    return audio;
}

void WfmDemodulator::reset() {
    i_history_.fill(0.0F);
    q_history_.fill(0.0F);
    history_position_ = 0;
    rf_decimation_phase_ = 0;
    previous_i_ = 0.0F;
    previous_q_ = 0.0F;
    discriminator_sum_ = 0.0F;
    deemphasis_state_ = 0.0F;
    dc_estimate_ = 0.0F;
    audio_decimation_phase_ = 0;
    has_previous_ = false;
}

} // namespace dsp
