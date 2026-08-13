// SPDX-License-Identifier: MIT

#pragma once

#include "base_screen.h"

#include <array>

namespace screen {

class SettingsScreen : public BaseScreen {
public:
    SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

protected:
    void build_content(lv_obj_t* content) override;
    void handle_key(platform::AppKey key, bool repeated) override;

private:
    static constexpr size_t kRowCount = 4;

    void move_selection(int direction);
    void adjust_selection(int direction);
    void refresh_rows();

    std::array<lv_obj_t*, kRowCount> rows_{};
    std::array<lv_obj_t*, kRowCount> labels_{};
    std::array<lv_obj_t*, kRowCount> values_{};
    size_t selected_row_{0};
};

} // namespace screen
