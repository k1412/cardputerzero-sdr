// SPDX-License-Identifier: MIT

#include "radio_model.h"

#include <algorithm>
#include <cstdint>

namespace model {

uint32_t RadioModel::frequency_hz() const {
    return frequency_hz_;
}

void RadioModel::set_frequency_hz(uint32_t frequency_hz) {
    frequency_hz_ = std::clamp(frequency_hz, kMinimumFrequencyHz, kMaximumFrequencyHz);
}

void RadioModel::tune(int direction) {
    if (direction == 0) {
        return;
    }

    const int64_t delta = static_cast<int64_t>(tuning_step_hz()) * (direction < 0 ? -1 : 1);
    const int64_t next = static_cast<int64_t>(frequency_hz_) + delta;
    set_frequency_hz(static_cast<uint32_t>(std::clamp<int64_t>(
        next,
        kMinimumFrequencyHz,
        kMaximumFrequencyHz)));
}

uint32_t RadioModel::tuning_step_hz() const {
    return kTuningStepsHz[tuning_step_index_];
}

size_t RadioModel::tuning_step_index() const {
    return tuning_step_index_;
}

void RadioModel::set_tuning_step_index(size_t index) {
    tuning_step_index_ = std::min(index, kTuningStepsHz.size() - 1);
}

void RadioModel::cycle_tuning_step(int direction) {
    if (direction == 0) {
        return;
    }
    const auto count = static_cast<int>(kTuningStepsHz.size());
    const auto current = static_cast<int>(tuning_step_index_);
    tuning_step_index_ = static_cast<size_t>((current + (direction < 0 ? count - 1 : 1)) % count);
}

bool RadioModel::muted() const {
    return muted_;
}

void RadioModel::set_muted(bool muted) {
    muted_ = muted;
}

void RadioModel::toggle_muted() {
    muted_ = !muted_;
}

bool RadioModel::automatic_gain() const {
    return automatic_gain_;
}

int RadioModel::gain_tenths_db() const {
    return gain_tenths_db_;
}

void RadioModel::set_gain(bool automatic_gain, int gain_tenths_db) {
    automatic_gain_ = automatic_gain;
    gain_tenths_db_ = std::clamp(gain_tenths_db, 0, 490);
}

void RadioModel::toggle_gain_mode() {
    automatic_gain_ = !automatic_gain_;
}

void RadioModel::adjust_gain(int direction) {
    automatic_gain_ = false;
    gain_tenths_db_ = std::clamp(gain_tenths_db_ + (direction < 0 ? -10 : 10), 0, 490);
}

bool RadioModel::dark_mode() const {
    return dark_mode_;
}

void RadioModel::set_dark_mode(bool enabled) {
    dark_mode_ = enabled;
}

void RadioModel::toggle_dark_mode() {
    dark_mode_ = !dark_mode_;
}

AppPage RadioModel::current_page() const {
    return current_page_;
}

void RadioModel::set_current_page(AppPage page) {
    current_page_ = page;
}

void RadioModel::toggle_page() {
    current_page_ = current_page_ == AppPage::Radio ? AppPage::Settings : AppPage::Radio;
}

SourceState RadioModel::source_state() const {
    return source_state_;
}

void RadioModel::set_source_state(SourceState state) {
    source_state_ = state;
}

size_t RadioModel::locale_index() const {
    return locale_index_;
}

void RadioModel::set_locale_index(size_t index, size_t locale_count) {
    locale_index_ = locale_count == 0 ? 0 : std::min(index, locale_count - 1);
}

void RadioModel::cycle_locale(size_t locale_count, int direction) {
    if (locale_count == 0 || direction == 0) {
        return;
    }
    const auto count = static_cast<int>(locale_count);
    const auto current = static_cast<int>(locale_index_);
    locale_index_ = static_cast<size_t>((current + (direction < 0 ? count - 1 : 1)) % count);
}

} // namespace model
