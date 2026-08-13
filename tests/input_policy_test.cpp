// SPDX-License-Identifier: MIT

#include "app_key.h"

#include <cassert>

int main() {
    using platform::AppInputContext;
    using platform::AppKey;
    using platform::AppKeyRepeatState;
    using platform::AppKeyboardCapability;
    using platform::accepts_app_key_repeat;
    using platform::app_keyboard_capability;
    using platform::app_key_repeat_due;
    using platform::supports_zero_sdr_controls;

    assert(!app_key_repeat_due(499, 0, 0));
    assert(app_key_repeat_due(500, 0, 0));
    assert(!app_key_repeat_due(549, 0, 500));
    assert(app_key_repeat_due(550, 0, 500));
    assert(app_key_repeat_due(40, 0xfffffe0cU, 0xfffffff2U));

    AppKeyRepeatState repeat;
    assert(!repeat.due(1'000));
    repeat.press(42, 1'000);
    assert(repeat.armed && repeat.key == 42);
    assert(!repeat.due(1'499));
    assert(repeat.due(1'500));
    repeat.emitted(1'500);
    assert(!repeat.due(1'549));
    assert(repeat.due(1'550));
    repeat.observed_repeat(42, 1'575);
    assert(!repeat.due(1'624));
    assert(repeat.due(1'625));
    repeat.release(41);
    assert(repeat.armed);
    repeat.release(42);
    assert(!repeat.armed && !repeat.due(2'000));

    const uint32_t full_keyboard =
        app_keyboard_capability(AppKeyboardCapability::Navigation) |
        app_keyboard_capability(AppKeyboardCapability::ConfirmAndBack) |
        app_keyboard_capability(AppKeyboardCapability::RadioShortcuts) |
        app_keyboard_capability(AppKeyboardCapability::DirectEntry);
    assert(supports_zero_sdr_controls(full_keyboard));
    assert(!supports_zero_sdr_controls(0));
    assert(!supports_zero_sdr_controls(
        full_keyboard & ~app_keyboard_capability(AppKeyboardCapability::Navigation)));
    assert(!supports_zero_sdr_controls(
        full_keyboard & ~app_keyboard_capability(AppKeyboardCapability::ConfirmAndBack)));
    assert(!supports_zero_sdr_controls(
        full_keyboard & ~app_keyboard_capability(AppKeyboardCapability::RadioShortcuts)));
    assert(!supports_zero_sdr_controls(
        full_keyboard & ~app_keyboard_capability(AppKeyboardCapability::DirectEntry)));

    assert(accepts_app_key_repeat(AppInputContext::Radio, AppKey::Left));
    assert(accepts_app_key_repeat(AppInputContext::Radio, AppKey::Right));
    assert(!accepts_app_key_repeat(AppInputContext::Radio, AppKey::Up));
    assert(!accepts_app_key_repeat(AppInputContext::Radio, AppKey::Confirm));
    assert(!accepts_app_key_repeat(AppInputContext::Radio, AppKey::Mute));
    assert(!accepts_app_key_repeat(AppInputContext::Radio, AppKey::Digit1));

    assert(accepts_app_key_repeat(AppInputContext::Settings, AppKey::Up));
    assert(accepts_app_key_repeat(AppInputContext::Settings, AppKey::Down));
    assert(!accepts_app_key_repeat(AppInputContext::Settings, AppKey::Left));
    assert(!accepts_app_key_repeat(AppInputContext::Settings, AppKey::Confirm));
    assert(!accepts_app_key_repeat(AppInputContext::Settings, AppKey::Language));

    for (int value = static_cast<int>(AppKey::None);
         value <= static_cast<int>(AppKey::Delete);
         ++value) {
        assert(!accepts_app_key_repeat(AppInputContext::DirectEntry,
                                       static_cast<AppKey>(value)));
    }
}
