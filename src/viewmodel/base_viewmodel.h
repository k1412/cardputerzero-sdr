/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "radio_model.h"
#include "synthetic_spectrum.h"
#include "translations.h"
#include "radio_session.h"

#include "lvgl.h"

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* title_subject();
    lv_subject_t* frequency_subject();
    lv_subject_t* source_subject();
    lv_subject_t* gain_subject();
    lv_subject_t* step_subject();
    lv_subject_t* locale_name_subject();
    lv_subject_t* muted_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    lv_subject_t* locale_subject();
    lv_subject_t* quit_requested_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    model::AppPage current_page() const;
    void show_radio_page();
    void show_settings_page();
    void toggle_page();
    void tune(int direction);
    void cycle_tuning_step(int direction);
    void toggle_gain_mode();
    void adjust_gain(int direction);
    void toggle_muted();
    void cycle_locale(int direction = 1);
    void set_locale(i18n::Locale locale);
    void restore_radio_settings(uint32_t frequency_hz,
                                size_t tuning_step_index,
                                bool automatic_gain,
                                int gain_tenths_db,
                                bool muted);
    void request_quit();

    uint32_t frequency_hz() const;
    size_t tuning_step_index() const;
    bool automatic_gain() const;
    int gain_tenths_db() const;
    bool is_muted() const;
    i18n::Locale locale() const;
    const char* text(i18n::Text text) const;
    const char* locale_font_asset() const;
    dsp::SpectrumFrame next_spectrum_frame();

private:
    void publish_all();
    void publish_radio_state();
    void publish_locale_state();

    model::RadioModel model_;
    dsp::SyntheticSpectrum synthetic_spectrum_;
    device::RadioSession radio_session_;
    device::RadioSessionState published_session_state_{device::RadioSessionState::Stopped};
    reactive::StringSubject<48> title_subject_;
    reactive::StringSubject<32> frequency_subject_;
    reactive::StringSubject<32> source_subject_;
    reactive::StringSubject<32> gain_subject_;
    reactive::StringSubject<32> step_subject_;
    reactive::StringSubject<48> locale_name_subject_;
    reactive::BoolSubject muted_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::IntSubject locale_subject_;
    reactive::BoolSubject quit_requested_subject_;
};

} // namespace viewmodel
