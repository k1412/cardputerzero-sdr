// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsp {

class WfmDemodulator {
public:
    static constexpr uint32_t kInputSampleRateHz = 2'048'000;
    static constexpr uint32_t kAudioSampleRateHz = 32'000;
    static constexpr uint32_t kRfDecimation = 4;
    static constexpr uint32_t kDemodSampleRateHz = kInputSampleRateHz / kRfDecimation;
    static constexpr uint32_t kAudioDecimation = kDemodSampleRateHz / kAudioSampleRateHz;

    // Produces mono signed 16-bit PCM at kAudioSampleRateHz.
    std::vector<int16_t> process(const uint8_t* iq_bytes, size_t byte_count);
    void reset();

private:
    static constexpr size_t kRfFilterTaps = 33;
    std::array<float, kRfFilterTaps> i_history_{};
    std::array<float, kRfFilterTaps> q_history_{};
    size_t history_position_{0};
    uint32_t rf_decimation_phase_{0};
    float previous_i_{0.0F};
    float previous_q_{0.0F};
    float discriminator_sum_{0.0F};
    float deemphasis_state_{0.0F};
    uint32_t audio_decimation_phase_{0};
    bool has_previous_{false};
};

} // namespace dsp
