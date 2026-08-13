#pragma once

#include "lvgl.h"

#include <cstddef>

namespace platform {

constexpr size_t kNavKeyCount = 5;

enum class AppKey {
    None = 0,
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back,
    Gain,
    Mute,
    Language,
    Theme,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    Decimal,
    Delete,
};

int app_key_digit(AppKey key);

using AppKeyHandler = void (*)(AppKey key, bool repeated, void* user_data);

void init_key_input(lv_display_t* display);
void attach_key_router(lv_indev_t* indev);
void set_app_key_handler(AppKeyHandler handler, void* user_data);
void clear_app_key_handler(void* user_data);
void set_nav_shortcut_mode(bool enabled);
void register_nav_button(size_t index, lv_obj_t* button);
void unregister_nav_button(size_t index, lv_obj_t* button);

} // namespace platform
