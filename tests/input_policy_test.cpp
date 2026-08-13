// SPDX-License-Identifier: MIT

#include "app_key.h"

#include <cassert>

int main() {
    using platform::AppInputContext;
    using platform::AppKey;
    using platform::accepts_app_key_repeat;

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
