// SPDX-License-Identifier: MIT

#pragma once

namespace platform {

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

enum class AppInputContext {
    Radio,
    DirectEntry,
    Settings,
};

constexpr bool accepts_app_key_repeat(AppInputContext context, AppKey key) {
    switch (context) {
        case AppInputContext::Radio:
            return key == AppKey::Left || key == AppKey::Right;
        case AppInputContext::Settings:
            return key == AppKey::Up || key == AppKey::Down;
        case AppInputContext::DirectEntry:
            return false;
    }
    return false;
}

} // namespace platform
