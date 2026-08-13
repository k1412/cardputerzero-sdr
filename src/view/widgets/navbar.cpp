// SPDX-License-Identifier: MIT

#include "navbar.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "ui_const.h"

#include <array>
#include <string>

namespace view::widgets {

NavBar::NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseWidgets(parent), view_model_(view_model), assets_(assets) {}

NavBar::~NavBar() {
    if (page_observer_) {
        lv_observer_remove(page_observer_);
        page_observer_ = nullptr;
    }
    if (locale_observer_) {
        lv_observer_remove(locale_observer_);
        locale_observer_ = nullptr;
    }
}

void NavBar::build() {
    if (core_obj_) {
        return;
    }

    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_set_size(core_obj_, LV_PCT(100), kNavBarHeight);
    lv_obj_align(core_obj_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(core_obj_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(core_obj_, 7, 0);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    reactive::bind_theme(core_obj_, view_model_.dark_mode_subject(), reactive::ThemeRole::Bar);

    auto* key_font = assets_.load_font("inter-semibold.ttf", 10);
    if (!key_font) key_font = const_cast<lv_font_t*>(&lv_font_montserrat_12);
    for (size_t index = 0; index < hint_labels_.size(); ++index) {
        auto*& label = hint_labels_[index];
        label = lv_label_create(core_obj_);
        lv_obj_set_width(label, 100);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(label,
                                    index == 0 ? LV_TEXT_ALIGN_LEFT
                                               : (index == 1 ? LV_TEXT_ALIGN_CENTER : LV_TEXT_ALIGN_RIGHT),
                                    0);
        lv_obj_set_style_text_font(label, key_font, 0);
        reactive::bind_theme(label, view_model_.dark_mode_subject(), reactive::ThemeRole::Text);
    }

    page_observer_ = reactive::observe_obj(core_obj_, view_model_.current_page_subject(), page_observer_cb, this);
    locale_observer_ = reactive::observe_obj(core_obj_, view_model_.locale_subject(), locale_observer_cb, this);
    refresh_hints();
}

void NavBar::refresh_hints() {
    const bool settings = view_model_.current_page() == model::AppPage::Settings;
    const std::array<std::string, 3> radio_hints = {
        std::string("F/X ") + view_model_.text(i18n::Text::Step),
        std::string("Z/C ") + view_model_.text(i18n::Text::Tune),
        std::string("ENT ") + view_model_.text(i18n::Text::Menu),
    };
    const std::array<std::string, 3> settings_hints = {
        std::string("F/X ") + view_model_.text(i18n::Text::Move),
        std::string("Z/C ") + view_model_.text(i18n::Text::Change),
        std::string("ENT ") + view_model_.text(i18n::Text::Back),
    };
    const auto& hints = settings ? settings_hints : radio_hints;
    const auto colors = view::palette(view_model_.is_dark_mode());
    auto* locale_font = assets_.load_font(view_model_.locale_font_asset(), 10);
    if (!locale_font) locale_font = assets_.load_font("inter-semibold.ttf", 10);
    if (!locale_font) locale_font = const_cast<lv_font_t*>(&lv_font_montserrat_12);
    for (size_t index = 0; index < hint_labels_.size(); ++index) {
        lv_label_set_text(hint_labels_[index], hints[index].c_str());
        lv_obj_set_style_text_font(hint_labels_[index], locale_font, 0);
        lv_obj_set_style_text_color(hint_labels_[index], index == 1 ? colors.primary : colors.text_disabled, 0);
    }
}

void NavBar::locale_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);
    auto* nav = static_cast<NavBar*>(lv_observer_get_user_data(observer));
    if (nav) {
        nav->refresh_hints();
    }
}

void NavBar::page_observer_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);
    auto* nav = static_cast<NavBar*>(lv_observer_get_user_data(observer));
    if (nav) {
        nav->refresh_hints();
    }
}

} // namespace view::widgets
