// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace model {

enum class AppPage : uint8_t {
    Radio = 0,
    Settings = 1,
};

enum class SourceState : uint8_t {
    Demo = 0,
    Connecting,
    Live,
    Missing,
    Error,
};

class RadioModel {
public:
    static constexpr uint32_t kMinimumFrequencyHz = 22'000'000;
    static constexpr uint32_t kMaximumFrequencyHz = 948'600'000;
    static constexpr uint32_t kDefaultFrequencyHz = 97'400'000;
    static constexpr std::array<uint32_t, 6> kTuningStepsHz = {
        10'000,
        50'000,
        100'000,
        200'000,
        500'000,
        1'000'000,
    };

    uint32_t frequency_hz() const;
    void set_frequency_hz(uint32_t frequency_hz);
    void tune(int direction);

    uint32_t tuning_step_hz() const;
    size_t tuning_step_index() const;
    void set_tuning_step_index(size_t index);
    void cycle_tuning_step(int direction);

    bool muted() const;
    void set_muted(bool muted);
    void toggle_muted();

    bool automatic_gain() const;
    int gain_tenths_db() const;
    void set_gain(bool automatic_gain, int gain_tenths_db);
    void toggle_gain_mode();
    void adjust_gain(int direction);

    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    AppPage current_page() const;
    void set_current_page(AppPage page);
    void toggle_page();

    SourceState source_state() const;
    void set_source_state(SourceState state);

    size_t locale_index() const;
    void set_locale_index(size_t index, size_t locale_count);
    void cycle_locale(size_t locale_count, int direction = 1);

private:
    uint32_t frequency_hz_{kDefaultFrequencyHz};
    size_t tuning_step_index_{3};
    bool muted_{false};
    bool automatic_gain_{true};
    int gain_tenths_db_{200};
    bool dark_mode_{true};
    AppPage current_page_{AppPage::Radio};
    SourceState source_state_{SourceState::Demo};
    size_t locale_index_{0};
};

} // namespace model
