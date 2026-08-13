// SPDX-License-Identifier: MIT

#include "radio_model.h"
#include "frequency_entry.h"

#include <cassert>

int main() {
    uint32_t parsed_frequency = 0;
    assert(model::parse_frequency_mhz("97.4", parsed_frequency));
    assert(parsed_frequency == 97'400'000);
    assert(model::parse_frequency_mhz("103.900", parsed_frequency));
    assert(parsed_frequency == 103'900'000);
    assert(model::parse_frequency_mhz("22", parsed_frequency));
    assert(parsed_frequency == model::RadioModel::kMinimumFrequencyHz);
    assert(model::parse_frequency_mhz("948.6", parsed_frequency));
    assert(parsed_frequency == model::RadioModel::kMaximumFrequencyHz);
    assert(!model::parse_frequency_mhz("21.999", parsed_frequency));
    assert(!model::parse_frequency_mhz("948.601", parsed_frequency));
    assert(!model::parse_frequency_mhz("97.4000", parsed_frequency));
    assert(!model::parse_frequency_mhz("9x.4", parsed_frequency));

    model::RadioModel radio;
    assert(radio.frequency_hz() == model::RadioModel::kDefaultFrequencyHz);
    assert(radio.tuning_step_hz() == 200'000);

    radio.tune(1);
    assert(radio.frequency_hz() == 97'600'000);
    radio.tune(-1);
    assert(radio.frequency_hz() == model::RadioModel::kDefaultFrequencyHz);

    radio.set_frequency_hz(0);
    assert(radio.frequency_hz() == model::RadioModel::kMinimumFrequencyHz);
    radio.tune(-1);
    assert(radio.frequency_hz() == model::RadioModel::kMinimumFrequencyHz);

    radio.set_frequency_hz(UINT32_MAX);
    assert(radio.frequency_hz() == model::RadioModel::kMaximumFrequencyHz);
    radio.tune(1);
    assert(radio.frequency_hz() == model::RadioModel::kMaximumFrequencyHz);

    radio.set_tuning_step_index(99);
    assert(radio.tuning_step_hz() == 1'000'000);
    radio.cycle_tuning_step(1);
    assert(radio.tuning_step_hz() == 10'000);
    radio.cycle_tuning_step(-1);
    assert(radio.tuning_step_hz() == 1'000'000);

    assert(radio.automatic_gain());
    radio.adjust_gain(1);
    assert(!radio.automatic_gain());
    assert(radio.gain_tenths_db() == 210);
    radio.set_gain(true, 999);
    assert(radio.automatic_gain());
    assert(radio.gain_tenths_db() == 490);
    radio.set_muted(true);
    assert(radio.muted());

    radio.toggle_page();
    assert(radio.current_page() == model::AppPage::Settings);
    radio.cycle_locale(10, -1);
    assert(radio.locale_index() == 9);
}
