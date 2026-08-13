// SPDX-License-Identifier: MIT

#pragma once

#include "base_screen.h"
#include "synthetic_spectrum.h"

#include <array>
#include <cstdint>

namespace screen {

class RadioScreen : public BaseScreen {
public:
    RadioScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~RadioScreen() override;

protected:
    void build_content(lv_obj_t* content) override;
    void handle_key(platform::AppKey key, bool repeated) override;

private:
    static constexpr int kWaterfallWidth = 316;
    static constexpr int kWaterfallHeight = 19;

    static void refresh_timer_cb(lv_timer_t* timer);
    static void muted_observer_cb(lv_observer_t* observer, lv_subject_t* subject);
    void refresh_locale();
    void refresh_spectrum();
    void update_waterfall(const dsp::SpectrumFrame& frame);

    lv_obj_t* chart_{nullptr};
    lv_chart_series_t* series_{nullptr};
    lv_obj_t* waterfall_{nullptr};
    lv_obj_t* source_label_{nullptr};
    lv_obj_t* gain_label_{nullptr};
    lv_obj_t* step_label_{nullptr};
    lv_obj_t* muted_label_{nullptr};
    lv_timer_t* refresh_timer_{nullptr};
    std::array<int32_t, dsp::kSpectrumBinCount> chart_values_{};
    std::array<uint16_t, kWaterfallWidth * kWaterfallHeight> waterfall_pixels_{};
};

} // namespace screen
