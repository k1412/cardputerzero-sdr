// SPDX-License-Identifier: MIT

#pragma once

#include "base_widget.h"
#include "base_viewmodel.h"

#include <array>

namespace app {
class AssetManager;
}

namespace view::widgets {

class NavBar : public BaseWidgets {
public:
    NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~NavBar() override;

    void build() override;

private:
    void refresh_hints();
    static void page_observer_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void locale_observer_cb(lv_observer_t* observer, lv_subject_t* subject);

    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    std::array<lv_obj_t*, 3> hint_labels_{};
    lv_observer_t* page_observer_{nullptr};
    lv_observer_t* locale_observer_{nullptr};
};

} // namespace view::widgets
