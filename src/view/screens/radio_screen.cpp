// SPDX-License-Identifier: MIT

#include "radio_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "frequency_entry.h"
#include "theme.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

namespace screen {
namespace {

lv_color_t waterfall_color(uint8_t level) {
    if (level < 25) {
        return lv_color_mix(lv_color_hex(0x12395a), lv_color_hex(0x03080d), level * 10);
    }
    if (level < 58) {
        return lv_color_mix(lv_color_hex(0x35dcc8), lv_color_hex(0x12395a), (level - 25) * 7);
    }
    return lv_color_mix(lv_color_hex(0xffd166), lv_color_hex(0x35dcc8), (level - 58) * 6);
}

void configure_pill(lv_obj_t* label, lv_color_t color, const lv_font_t* font) {
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_bg_color(label, color, 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_20, 0);
    lv_obj_set_style_pad_hor(label, 5, 0);
    lv_obj_set_style_pad_ver(label, 1, 0);
    lv_obj_set_style_radius(label, 4, 0);
}

} // namespace

RadioScreen::RadioScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

RadioScreen::~RadioScreen() {
    if (refresh_timer_) {
        lv_timer_delete(refresh_timer_);
        refresh_timer_ = nullptr;
    }
}

void RadioScreen::build_content(lv_obj_t* content) {
    const auto colors = view::palette(view_model().is_dark_mode());
    auto* frequency_font = assets().load_font("inter-semibold.ttf", 29);
    auto* label_font = assets().load_font("inter-medium.ttf", 11);
    auto* small_font = assets().load_font("inter-regular.ttf", 10);
    if (!frequency_font) frequency_font = const_cast<lv_font_t*>(&lv_font_montserrat_28);
    if (!label_font) label_font = const_cast<lv_font_t*>(&lv_font_montserrat_12);
    if (!small_font) small_font = const_cast<lv_font_t*>(&lv_font_montserrat_12);

    auto* frequency = lv_label_create(content);
    lv_label_bind_text(frequency, view_model().frequency_subject(), nullptr);
    lv_obj_set_style_text_font(frequency, frequency_font, 0);
    lv_obj_set_style_text_color(frequency, colors.text, 0);
    lv_obj_align(frequency, LV_ALIGN_TOP_LEFT, 7, -5);

    auto* unit = lv_label_create(content);
    lv_label_set_text(unit, "MHz");
    lv_obj_set_style_text_font(unit, label_font, 0);
    lv_obj_set_style_text_color(unit, colors.text_disabled, 0);
    lv_obj_align(unit, LV_ALIGN_TOP_LEFT, 119, 12);

    auto* mode = lv_label_create(content);
    lv_label_set_text(mode, "WFM");
    configure_pill(mode, colors.primary, label_font);
    lv_obj_align(mode, LV_ALIGN_TOP_LEFT, 159, 5);

    source_label_ = lv_label_create(content);
    lv_label_bind_text(source_label_, view_model().source_subject(), nullptr);
    lv_label_set_long_mode(source_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_width(source_label_, 72);
    configure_pill(source_label_, colors.info, label_font);
    lv_obj_set_style_text_align(source_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(source_label_, LV_ALIGN_TOP_RIGHT, -6, 5);

    gain_label_ = lv_label_create(content);
    lv_label_bind_text(gain_label_, view_model().gain_subject(), nullptr);
    lv_obj_set_style_text_font(gain_label_, small_font, 0);
    lv_obj_set_style_text_color(gain_label_, colors.text_disabled, 0);
    lv_obj_align(gain_label_, LV_ALIGN_TOP_LEFT, 8, 34);

    step_label_ = lv_label_create(content);
    lv_label_bind_text(step_label_, view_model().step_subject(), nullptr);
    lv_obj_set_style_text_font(step_label_, small_font, 0);
    lv_obj_set_style_text_color(step_label_, colors.text_disabled, 0);
    lv_obj_align(step_label_, LV_ALIGN_TOP_MID, 4, 34);

    muted_label_ = lv_label_create(content);
    lv_obj_set_style_text_font(muted_label_, small_font, 0);
    lv_obj_set_style_text_color(muted_label_, lv_color_hex(0xff6b6b), 0);
    lv_obj_align(muted_label_, LV_ALIGN_TOP_RIGHT, -8, 34);
    lv_label_set_text(muted_label_, view_model().is_muted() ? view_model().text(i18n::Text::Muted) : "");
    lv_subject_add_observer_obj(view_model().muted_subject(), muted_observer_cb, muted_label_, this);

    chart_ = lv_chart_create(content);
    lv_obj_remove_style_all(chart_);
    lv_obj_set_size(chart_, 316, 51);
    lv_obj_align(chart_, LV_ALIGN_BOTTOM_MID, 0, -kWaterfallHeight);
    lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_, static_cast<uint32_t>(dsp::kSpectrumBinCount));
    lv_chart_set_axis_range(chart_, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(chart_, 3, 5);
    lv_obj_set_style_bg_color(chart_, lv_color_hex(0x06111b), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart_, lv_color_hex(0x183247), LV_PART_MAIN);
    lv_obj_set_style_line_opa(chart_, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_line_width(chart_, 1, LV_PART_MAIN);
    lv_obj_set_style_size(chart_, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart_, 2, LV_PART_ITEMS);
    lv_obj_set_style_radius(chart_, 0, 0);
    series_ = lv_chart_add_series(chart_, colors.primary, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_series_ext_y_array(chart_, series_, chart_values_.data());

    waterfall_ = lv_canvas_create(content);
    lv_canvas_set_buffer(waterfall_,
                         waterfall_pixels_.data(),
                         kWaterfallWidth,
                         kWaterfallHeight,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_align(waterfall_, LV_ALIGN_BOTTOM_MID, 0, 0);

    direct_panel_ = lv_obj_create(content);
    lv_obj_set_size(direct_panel_, 294, 78);
    lv_obj_align(direct_panel_, LV_ALIGN_CENTER, 0, 1);
    lv_obj_set_style_radius(direct_panel_, 7, 0);
    lv_obj_set_style_border_width(direct_panel_, 1, 0);
    lv_obj_set_style_pad_all(direct_panel_, 5, 0);
    lv_obj_clear_flag(direct_panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(direct_panel_, LV_OBJ_FLAG_HIDDEN);

    direct_title_ = lv_label_create(direct_panel_);
    lv_obj_set_width(direct_title_, LV_PCT(100));
    lv_obj_set_style_text_align(direct_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(direct_title_, LV_ALIGN_TOP_MID, 0, 0);

    direct_value_ = lv_label_create(direct_panel_);
    lv_obj_set_width(direct_value_, LV_PCT(100));
    auto* direct_value_font = assets().load_font("inter-semibold.ttf", 24);
    if (!direct_value_font) direct_value_font = const_cast<lv_font_t*>(&lv_font_montserrat_24);
    lv_obj_set_style_text_font(direct_value_, direct_value_font, 0);
    lv_obj_set_style_text_align(direct_value_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(direct_value_, LV_ALIGN_CENTER, 0, 0);

    direct_hint_ = lv_label_create(direct_panel_);
    lv_obj_set_width(direct_hint_, LV_PCT(100));
    lv_obj_set_style_text_align(direct_hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(direct_hint_, LV_ALIGN_BOTTOM_MID, 0, 0);

    refresh_spectrum();
    refresh_locale();
#if USE_DESKTOP
    if (const char* preview = std::getenv("ZERO_SDR_DIRECT_ENTRY")) {
        for (const char* cursor = preview; *cursor != '\0'; ++cursor) {
            if (*cursor >= '0' && *cursor <= '9') append_direct_digit(*cursor - '0');
            else if (*cursor == '.') append_direct_decimal();
        }
    }
#endif
    refresh_timer_ = lv_timer_create(refresh_timer_cb, 120, this);
}

void RadioScreen::handle_key(platform::AppKey key, bool repeated) {
    const int digit = platform::app_key_digit(key);
    if (digit >= 0) {
        if (!repeated) append_direct_digit(digit);
        return;
    }
    if (key == platform::AppKey::Decimal) {
        if (!repeated) append_direct_decimal();
        return;
    }
    if (direct_entry_active_) {
        if (repeated) return;
        switch (key) {
            case platform::AppKey::Delete: delete_direct_character(); break;
            case platform::AppKey::Confirm: commit_direct_entry(); break;
            case platform::AppKey::Back: cancel_direct_entry(); break;
            default: break;
        }
        return;
    }
    switch (key) {
        case platform::AppKey::Left: view_model().tune(-1); break;
        case platform::AppKey::Right: view_model().tune(1); break;
        case platform::AppKey::Up: view_model().cycle_tuning_step(1); break;
        case platform::AppKey::Down: view_model().cycle_tuning_step(-1); break;
        case platform::AppKey::Confirm: view_model().show_settings_page(); break;
        case platform::AppKey::Back: view_model().request_quit(); break;
        case platform::AppKey::Gain: view_model().toggle_gain_mode(); break;
        case platform::AppKey::Mute: view_model().toggle_muted(); break;
        case platform::AppKey::Language: view_model().cycle_locale(); refresh_locale(); break;
        case platform::AppKey::Theme: view_model().toggle_dark_mode(); break;
        case platform::AppKey::Digit0:
        case platform::AppKey::Digit1:
        case platform::AppKey::Digit2:
        case platform::AppKey::Digit3:
        case platform::AppKey::Digit4:
        case platform::AppKey::Digit5:
        case platform::AppKey::Digit6:
        case platform::AppKey::Digit7:
        case platform::AppKey::Digit8:
        case platform::AppKey::Digit9:
        case platform::AppKey::Decimal:
        case platform::AppKey::Delete:
        case platform::AppKey::None: break;
    }
}

void RadioScreen::begin_direct_entry() {
    direct_entry_.clear();
    direct_entry_error_ = false;
    direct_entry_active_ = true;
    lv_obj_remove_flag(direct_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(direct_panel_);
    refresh_direct_entry();
}

void RadioScreen::append_direct_digit(int digit) {
    if (!direct_entry_active_) begin_direct_entry();
    const auto decimal = direct_entry_.find('.');
    const auto whole_length = decimal == std::string::npos ? direct_entry_.size() : decimal;
    const auto fraction_length = decimal == std::string::npos
        ? 0
        : direct_entry_.size() - decimal - 1;
    if ((decimal == std::string::npos && whole_length >= 3) ||
        (decimal != std::string::npos && fraction_length >= 3)) {
        return;
    }
    direct_entry_.push_back(static_cast<char>('0' + digit));
    direct_entry_error_ = false;
    refresh_direct_entry();
}

void RadioScreen::append_direct_decimal() {
    if (!direct_entry_active_) begin_direct_entry();
    if (direct_entry_.find('.') != std::string::npos) return;
    if (direct_entry_.empty()) direct_entry_ = "0";
    direct_entry_.push_back('.');
    direct_entry_error_ = false;
    refresh_direct_entry();
}

void RadioScreen::delete_direct_character() {
    if (!direct_entry_.empty()) direct_entry_.pop_back();
    direct_entry_error_ = false;
    refresh_direct_entry();
}

void RadioScreen::commit_direct_entry() {
    uint32_t frequency_hz = 0;
    if (!model::parse_frequency_mhz(direct_entry_, frequency_hz)) {
        direct_entry_error_ = true;
        refresh_direct_entry();
        return;
    }
    view_model().set_frequency_hz(frequency_hz);
    cancel_direct_entry();
}

void RadioScreen::cancel_direct_entry() {
    direct_entry_.clear();
    direct_entry_error_ = false;
    direct_entry_active_ = false;
    lv_obj_add_flag(direct_panel_, LV_OBJ_FLAG_HIDDEN);
}

void RadioScreen::refresh_direct_entry() {
    if (!direct_panel_) return;
    const auto colors = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_color(direct_panel_, colors.surface, 0);
    lv_obj_set_style_bg_opa(direct_panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(direct_panel_, direct_entry_error_ ? lv_color_hex(0xff6b6b) : colors.primary, 0);
    lv_obj_set_style_text_color(direct_title_, direct_entry_error_ ? lv_color_hex(0xff6b6b) : colors.primary, 0);
    lv_obj_set_style_text_color(direct_value_, colors.text, 0);
    lv_obj_set_style_text_color(direct_hint_, colors.text_disabled, 0);

    const std::string title = direct_entry_error_
        ? "22.000-948.600 MHz"
        : std::string(view_model().text(i18n::Text::Tune)) + " MHz";
    const std::string value = direct_entry_.empty() ? "_" : direct_entry_ + "_";
    const std::string hint = std::string("ENT ") + view_model().text(i18n::Text::Select) +
                             " / ESC " + view_model().text(i18n::Text::Back);
    lv_label_set_text(direct_title_, title.c_str());
    lv_label_set_text(direct_value_, value.c_str());
    lv_label_set_text(direct_hint_, hint.c_str());
}

void RadioScreen::refresh_timer_cb(lv_timer_t* timer) {
    auto* screen = static_cast<RadioScreen*>(lv_timer_get_user_data(timer));
    if (screen) {
        screen->refresh_spectrum();
    }
}

void RadioScreen::muted_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    auto* screen = static_cast<RadioScreen*>(lv_observer_get_user_data(observer));
    auto* label = lv_observer_get_target_obj(observer);
    if (!screen || !label) {
        return;
    }
    lv_label_set_text(label,
                      lv_subject_get_int(subject) ? screen->view_model().text(i18n::Text::Muted) : "");
}

void RadioScreen::refresh_locale() {
    auto* locale_font = assets().load_font(view_model().locale_font_asset(), 10);
    if (!locale_font) locale_font = assets().load_font("inter-regular.ttf", 10);
    if (!locale_font) locale_font = const_cast<lv_font_t*>(&lv_font_montserrat_12);

    for (auto* label : {source_label_, gain_label_, step_label_, muted_label_}) {
        if (label) {
            lv_obj_set_style_text_font(label, locale_font, 0);
        }
    }
    for (auto* label : {direct_title_, direct_hint_}) {
        if (label) lv_obj_set_style_text_font(label, locale_font, 0);
    }
    if (muted_label_) {
        lv_label_set_text(muted_label_,
                          view_model().is_muted() ? view_model().text(i18n::Text::Muted) : "");
    }
    if (direct_entry_active_) refresh_direct_entry();
}

void RadioScreen::refresh_spectrum() {
    const auto frame = view_model().next_spectrum_frame();
    for (size_t index = 0; index < frame.level.size(); ++index) {
        chart_values_[index] = frame.level[index];
    }
    lv_chart_refresh(chart_);
    update_waterfall(frame);
}

void RadioScreen::update_waterfall(const dsp::SpectrumFrame& frame) {
    const auto row_bytes = static_cast<size_t>(kWaterfallWidth) * sizeof(waterfall_pixels_[0]);
    std::memmove(waterfall_pixels_.data(),
                 waterfall_pixels_.data() + kWaterfallWidth,
                 row_bytes * static_cast<size_t>(kWaterfallHeight - 1));

    auto* row = waterfall_pixels_.data() + kWaterfallWidth * (kWaterfallHeight - 1);
    for (int x = 0; x < kWaterfallWidth; ++x) {
        const auto bin = std::min<size_t>(
            static_cast<size_t>(x) * frame.level.size() / kWaterfallWidth,
            frame.level.size() - 1);
        row[x] = lv_color_to_u16(waterfall_color(frame.level[bin]));
    }
    lv_obj_invalidate(waterfall_);
}

} // namespace screen
