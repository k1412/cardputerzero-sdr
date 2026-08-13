// SPDX-License-Identifier: MIT

#include "settings_screen.h"

#include "asset_manager.h"
#include "theme.h"

namespace screen {

SettingsScreen::SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void SettingsScreen::build_content(lv_obj_t* content) {
    auto* label_font = assets().load_font("inter-medium.ttf", 12);
    if (!label_font) label_font = const_cast<lv_font_t*>(&lv_font_montserrat_12);

    for (size_t index = 0; index < kRowCount; ++index) {
        auto* row = lv_obj_create(content);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, 310, 27);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, static_cast<int32_t>(index * 28));
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_pad_hor(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        rows_[index] = row;

        auto* label = lv_label_create(row);
        lv_obj_set_style_text_font(label, label_font, 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
        labels_[index] = label;

        auto* value = lv_label_create(row);
        lv_label_set_long_mode(value, LV_LABEL_LONG_DOT);
        lv_obj_set_width(value, 142);
        lv_obj_set_style_text_font(value, label_font, 0);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(value, LV_ALIGN_RIGHT_MID, 0, 0);
        values_[index] = value;
    }

    lv_label_bind_text(values_[3], view_model().audio_status_subject(), nullptr);

    refresh_rows();
}

void SettingsScreen::handle_key(platform::AppKey key, bool repeated) {
    if (repeated &&
        !platform::accepts_app_key_repeat(platform::AppInputContext::Settings, key)) {
        return;
    }
    switch (key) {
        case platform::AppKey::Up: move_selection(-1); break;
        case platform::AppKey::Down: move_selection(1); break;
        case platform::AppKey::Left: adjust_selection(-1); break;
        case platform::AppKey::Right: adjust_selection(1); break;
        case platform::AppKey::Confirm:
        case platform::AppKey::Back: view_model().show_radio_page(); break;
        case platform::AppKey::Gain: view_model().toggle_gain_mode(); refresh_rows(); break;
        case platform::AppKey::Mute: view_model().toggle_muted(); refresh_rows(); break;
        case platform::AppKey::Language: view_model().cycle_locale(); refresh_rows(); break;
        case platform::AppKey::Theme: view_model().toggle_dark_mode(); refresh_rows(); break;
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

void SettingsScreen::move_selection(int direction) {
    const auto count = static_cast<int>(kRowCount);
    selected_row_ = static_cast<size_t>((static_cast<int>(selected_row_) + (direction < 0 ? count - 1 : 1)) % count);
    refresh_rows();
}

void SettingsScreen::adjust_selection(int direction) {
    switch (selected_row_) {
        case 0: view_model().cycle_locale(direction); break;
        case 1: view_model().toggle_dark_mode(); break;
        case 2: view_model().adjust_gain(direction); break;
        case 3: view_model().toggle_muted(); break;
        default: break;
    }
    refresh_rows();
}

void SettingsScreen::refresh_rows() {
    const auto colors = view::palette(view_model().is_dark_mode());
    const std::array<const char*, kRowCount> labels = {
        view_model().text(i18n::Text::Language),
        view_model().text(i18n::Text::Theme),
        view_model().text(i18n::Text::Gain),
        view_model().text(i18n::Text::Audio),
    };
    const std::array<const char*, kRowCount> values = {
        i18n::locale_info(view_model().locale()).native_name,
        view_model().is_dark_mode() ? view_model().text(i18n::Text::Dark) : view_model().text(i18n::Text::Light),
        lv_subject_get_string(view_model().gain_subject()),
        lv_subject_get_string(view_model().audio_status_subject()),
    };

    auto* locale_font = assets().load_font(view_model().locale_font_asset(), 12);
    if (!locale_font) locale_font = assets().load_font("inter-medium.ttf", 12);
    if (!locale_font) locale_font = const_cast<lv_font_t*>(&lv_font_montserrat_12);

    for (size_t index = 0; index < kRowCount; ++index) {
        lv_label_set_text(labels_[index], labels[index]);
        if (index != 3) lv_label_set_text(values_[index], values[index]);
        lv_obj_set_style_text_font(labels_[index], locale_font, 0);
        lv_obj_set_style_text_font(values_[index], locale_font, 0);
        lv_obj_set_style_text_color(labels_[index], colors.text, 0);
        lv_obj_set_style_text_color(values_[index], index == selected_row_ ? colors.primary : colors.text_disabled, 0);
        lv_obj_set_style_bg_color(rows_[index], colors.primary, 0);
        lv_obj_set_style_bg_opa(rows_[index], index == selected_row_ ? LV_OPA_20 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(rows_[index], index == selected_row_ ? 1 : 0, 0);
        lv_obj_set_style_border_color(rows_[index], colors.primary, 0);
    }
}

} // namespace screen
