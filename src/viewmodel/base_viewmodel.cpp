/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

std::string rtl_sdr_library_path() {
    const char* path = std::getenv("ZERO_SDR_RTLSDR_LIBRARY");
    return path ? path : "";
}

std::string audio_library_path() {
    const char* path = std::getenv("ZERO_SDR_ALSA_LIBRARY");
    return path ? path : "";
}

std::string audio_device_name() {
    const char* name = std::getenv("ZERO_SDR_ALSA_DEVICE");
    return name ? name : "default";
}

} // namespace

BaseViewModel::BaseViewModel()
    : radio_session_(rtl_sdr_library_path(), audio_library_path(), audio_device_name()),
      title_subject_("Zero SDR"),
      frequency_subject_("97.400"),
      source_subject_("DEMO"),
      gain_subject_("AUTO GAIN"),
      step_subject_("200 kHz"),
      locale_name_subject_("English"),
      muted_subject_(model_.muted()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      locale_subject_(static_cast<int>(model_.locale_index())),
      quit_requested_subject_(false) {
    const bool force_demo = std::getenv("ZERO_SDR_DEMO") != nullptr;
#if USE_DESKTOP
    const bool enable_live = std::getenv("ZERO_SDR_LIVE") != nullptr;
#else
    const bool enable_live = true;
#endif
    if (enable_live && !force_demo) {
        radio_session_.request_frequency(model_.frequency_hz());
        radio_session_.request_gain(model_.automatic_gain(), model_.gain_tenths_db());
        radio_session_.start();
    }
}

BaseViewModel::~BaseViewModel() = default;

lv_subject_t* BaseViewModel::title_subject() {
    return title_subject_.native();
}

lv_subject_t* BaseViewModel::frequency_subject() {
    return frequency_subject_.native();
}

lv_subject_t* BaseViewModel::source_subject() {
    return source_subject_.native();
}

lv_subject_t* BaseViewModel::gain_subject() {
    return gain_subject_.native();
}

lv_subject_t* BaseViewModel::step_subject() {
    return step_subject_.native();
}

lv_subject_t* BaseViewModel::locale_name_subject() {
    return locale_name_subject_.native();
}

lv_subject_t* BaseViewModel::muted_subject() {
    return muted_subject_.native();
}

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
}

lv_subject_t* BaseViewModel::locale_subject() {
    return locale_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    model_.set_dark_mode(enabled);
    publish_all();
}

void BaseViewModel::toggle_dark_mode() {
    model_.toggle_dark_mode();
    publish_all();
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

void BaseViewModel::show_radio_page() {
    model_.set_current_page(model::AppPage::Radio);
    publish_all();
}

void BaseViewModel::show_settings_page() {
    model_.set_current_page(model::AppPage::Settings);
    publish_all();
}

void BaseViewModel::toggle_page() {
    model_.toggle_page();
    publish_all();
}

void BaseViewModel::tune(int direction) {
    model_.tune(direction);
    radio_session_.request_frequency(model_.frequency_hz());
    publish_radio_state();
}

void BaseViewModel::cycle_tuning_step(int direction) {
    model_.cycle_tuning_step(direction);
    publish_radio_state();
}

void BaseViewModel::toggle_gain_mode() {
    model_.toggle_gain_mode();
    radio_session_.request_gain(model_.automatic_gain(), model_.gain_tenths_db());
    publish_radio_state();
}

void BaseViewModel::adjust_gain(int direction) {
    model_.adjust_gain(direction);
    radio_session_.request_gain(model_.automatic_gain(), model_.gain_tenths_db());
    publish_radio_state();
}

void BaseViewModel::toggle_muted() {
    model_.toggle_muted();
    radio_session_.request_muted(model_.muted());
    publish_radio_state();
}

void BaseViewModel::cycle_locale(int direction) {
    model_.cycle_locale(i18n::kLocaleCount, direction);
    publish_locale_state();
    publish_radio_state();
}

void BaseViewModel::set_locale(i18n::Locale locale) {
    model_.set_locale_index(static_cast<size_t>(locale), i18n::kLocaleCount);
    publish_locale_state();
    publish_radio_state();
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

uint32_t BaseViewModel::frequency_hz() const {
    return model_.frequency_hz();
}

bool BaseViewModel::is_muted() const {
    return model_.muted();
}

i18n::Locale BaseViewModel::locale() const {
    return static_cast<i18n::Locale>(model_.locale_index());
}

const char* BaseViewModel::text(i18n::Text key) const {
    return i18n::translate(locale(), key);
}

const char* BaseViewModel::locale_font_asset() const {
    return i18n::locale_info(locale()).font_asset;
}

dsp::SpectrumFrame BaseViewModel::next_spectrum_frame() {
    if (radio_session_.started()) {
        const auto session_state = radio_session_.state();
        switch (session_state) {
            case device::RadioSessionState::Stopped:
            case device::RadioSessionState::Connecting:
                model_.set_source_state(model::SourceState::Connecting);
                break;
            case device::RadioSessionState::Live:
                model_.set_source_state(model::SourceState::Live);
                break;
            case device::RadioSessionState::Missing:
                model_.set_source_state(model::SourceState::Missing);
                break;
            case device::RadioSessionState::Error:
                model_.set_source_state(model::SourceState::Error);
                break;
        }
        if (session_state != published_session_state_) {
            published_session_state_ = session_state;
            publish_radio_state();
        }
        dsp::SpectrumFrame live_frame;
        if (radio_session_.latest_spectrum(live_frame)) return live_frame;
        return {};
    }
    return synthetic_spectrum_.next(model_.frequency_hz());
}

void BaseViewModel::publish_all() {
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
    publish_locale_state();
    publish_radio_state();
}

void BaseViewModel::publish_radio_state() {
    std::array<char, 32> buffer{};
    const auto mhz = model_.frequency_hz() / 1'000'000U;
    const auto khz = (model_.frequency_hz() % 1'000'000U) / 1'000U;
    std::snprintf(buffer.data(), buffer.size(), "%u.%03u", mhz, khz);
    frequency_subject_.set(buffer.data());

    switch (model_.source_state()) {
        case model::SourceState::Demo: source_subject_.set(text(i18n::Text::Demo)); break;
        case model::SourceState::Connecting: source_subject_.set(text(i18n::Text::Connecting)); break;
        case model::SourceState::Live: source_subject_.set(text(i18n::Text::Live)); break;
        case model::SourceState::Missing: source_subject_.set(text(i18n::Text::DeviceMissing)); break;
        case model::SourceState::Error: source_subject_.set(text(i18n::Text::DeviceError)); break;
    }

    if (model_.automatic_gain()) {
        gain_subject_.set(text(i18n::Text::GainAuto));
    }
    else {
        std::snprintf(buffer.data(), buffer.size(), "%.1f dB", model_.gain_tenths_db() / 10.0);
        gain_subject_.set(buffer.data());
    }

    const auto step = model_.tuning_step_hz();
    if (step >= 1'000'000U) {
        std::snprintf(buffer.data(), buffer.size(), "%u MHz", step / 1'000'000U);
    }
    else {
        std::snprintf(buffer.data(), buffer.size(), "%u kHz", step / 1'000U);
    }
    step_subject_.set(buffer.data());
    muted_subject_.set(model_.muted());
}

void BaseViewModel::publish_locale_state() {
    title_subject_.set(text(i18n::Text::AppTitle));
    locale_subject_.set(static_cast<int>(model_.locale_index()));
    locale_name_subject_.set(i18n::locale_info(locale()).native_name);
}

} // namespace viewmodel
