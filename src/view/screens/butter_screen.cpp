/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "butter_screen.h"

#include "asset_manager.h"
#include "bindings.h"

namespace screen {

ButterScreen::ButterScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void ButterScreen::build_content(lv_obj_t* content) {
    auto* page_label = lv_label_create(content);
    lv_label_bind_text(page_label, view_model().counter_subject(), "Counter: %d");
    auto* inter_semibold = assets().load_font("inter-semibold.ttf", 20);
    lv_obj_set_style_text_font(page_label, inter_semibold ? inter_semibold : &lv_font_montserrat_20, 0);
    lv_obj_center(page_label);
    reactive::bind_theme(page_label, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
}

} // namespace screen
