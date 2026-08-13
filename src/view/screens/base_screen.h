/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "navbar.h"
#include "base_viewmodel.h"
#include "linux_input.h"
#include "titlebar.h"
#include "lvgl.h"

#include <memory>

namespace app {
class AssetManager;
}

namespace screen {

class BaseScreen {
public:
    BaseScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    virtual ~BaseScreen();

    BaseScreen(const BaseScreen&) = delete;
    BaseScreen& operator=(const BaseScreen&) = delete;

    void init();
    lv_obj_t* root() const;

protected:
    virtual void build_content(lv_obj_t* content) = 0;
    virtual void handle_key(platform::AppKey key, bool repeated) = 0;

    viewmodel::BaseViewModel& view_model();
    app::AssetManager& assets();

private:
    static void app_key_handler(platform::AppKey key, bool repeated, void* user_data);

    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    lv_obj_t* root_{nullptr};
    lv_obj_t* content_{nullptr};
    std::unique_ptr<view::widgets::TitleBar> title_bar_;
    std::unique_ptr<view::widgets::NavBar> nav_bar_;
};

} // namespace screen
