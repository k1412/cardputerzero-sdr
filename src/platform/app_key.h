// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

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

enum class AppKeyboardCapability : uint32_t {
    Navigation = 1U << 0,
    ConfirmAndBack = 1U << 1,
    RadioShortcuts = 1U << 2,
    DirectEntry = 1U << 3,
};

constexpr uint32_t kAppKeyRepeatDelayMs = 500;
constexpr uint32_t kAppKeyRepeatRateMs = 50;

constexpr bool app_key_repeat_due(uint32_t now,
                                  uint32_t pressed_at,
                                  uint32_t last_repeat_at) {
    return static_cast<uint32_t>(now - pressed_at) >= kAppKeyRepeatDelayMs &&
           static_cast<uint32_t>(now - last_repeat_at) >= kAppKeyRepeatRateMs;
}

struct AppKeyRepeatState {
    bool armed{false};
    uint32_t key{0};
    uint32_t pressed_at{0};
    uint32_t last_repeat_at{0};

    constexpr void press(uint32_t pressed_key, uint32_t now) {
        armed = true;
        key = pressed_key;
        pressed_at = now;
        last_repeat_at = now;
    }

    constexpr void release(uint32_t released_key) {
        if (armed && key == released_key) armed = false;
    }

    constexpr void observed_repeat(uint32_t repeated_key, uint32_t now) {
        if (armed && key == repeated_key) last_repeat_at = now;
    }

    constexpr bool due(uint32_t now) const {
        return armed && app_key_repeat_due(now, pressed_at, last_repeat_at);
    }

    constexpr void emitted(uint32_t now) {
        last_repeat_at = now;
    }
};

constexpr uint32_t app_keyboard_capability(AppKeyboardCapability capability) {
    return static_cast<uint32_t>(capability);
}

constexpr bool supports_zero_sdr_controls(uint32_t capabilities) {
    constexpr uint32_t required =
        app_keyboard_capability(AppKeyboardCapability::Navigation) |
        app_keyboard_capability(AppKeyboardCapability::ConfirmAndBack) |
        app_keyboard_capability(AppKeyboardCapability::RadioShortcuts) |
        app_keyboard_capability(AppKeyboardCapability::DirectEntry);
    return (capabilities & required) == required;
}

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
