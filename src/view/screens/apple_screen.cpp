/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "apple_screen.h"

#include "asset_manager.h"
#include "bindings.h"

namespace screen {
namespace {

struct AppleTextFontBinding {
    lv_font_t* regular;
    lv_font_t* semibold;
};

void cleanup_font_binding(lv_event_t* event) {
    delete static_cast<AppleTextFontBinding*>(lv_event_get_user_data(event));
}

void apple_text_font_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* label = lv_observer_get_target_obj(observer);
    auto* fonts = static_cast<AppleTextFontBinding*>(lv_observer_get_user_data(observer));
    if (!label || !fonts) {
        return;
    }

    auto* font = lv_subject_get_int(subject) ? fonts->semibold : fonts->regular;
    lv_obj_set_style_text_font(label, font ? font : &lv_font_montserrat_20, 0);
}

void info_visible_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* label = lv_observer_get_target_obj(observer);
    if (!label) {
        return;
    }

    if (lv_subject_get_int(subject)) {
        lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
}

} // namespace

AppleScreen::AppleScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void AppleScreen::build_content(lv_obj_t* content) {
    auto* group = lv_obj_create(content);
    lv_obj_remove_style_all(group);
    lv_obj_set_size(group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(group, 8, 0);
    lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(group);

    auto* hello = lv_label_create(group);
    lv_label_bind_text(hello, view_model().greeting_subject(), nullptr);
    auto* fonts = new AppleTextFontBinding{
        assets().load_font("inter-regular.ttf", 20),
        assets().load_font("inter-semibold.ttf", 20),
    };
    auto* initial_font = lv_subject_get_int(view_model().bold_text_subject()) ? fonts->semibold : fonts->regular;
    lv_obj_set_style_text_font(hello, initial_font ? initial_font : &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(hello, cleanup_font_binding, LV_EVENT_DELETE, fonts);
    lv_subject_add_observer_obj(view_model().bold_text_subject(), apple_text_font_observer, hello, fonts);
    reactive::bind_theme(hello, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* info = lv_label_create(group);
    lv_label_set_text_fmt(info, "LVGL v%d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    auto* info_font = assets().load_font("inter-regular.ttf", 12);
    lv_obj_set_style_text_font(info, info_font ? info_font : &lv_font_montserrat_12, 0);
    if (!lv_subject_get_int(view_model().info_visible_subject())) {
        lv_obj_add_flag(info, LV_OBJ_FLAG_HIDDEN);
    }
    lv_subject_add_observer_obj(view_model().info_visible_subject(), info_visible_observer, info, nullptr);
    reactive::bind_theme(info, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
}

} // namespace screen
