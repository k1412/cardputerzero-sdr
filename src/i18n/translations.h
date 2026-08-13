// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace i18n {

enum class Locale : uint8_t {
    English = 0,
    SimplifiedChinese,
    TraditionalChinese,
    Spanish,
    Japanese,
    Korean,
    French,
    German,
    PortugueseBrazil,
    Russian,
};

enum class Text : uint8_t {
    AppTitle = 0,
    Demo,
    Connecting,
    Live,
    DeviceMissing,
    UsbAccess,
    ReconnectUsb,
    DeviceBusy,
    CloseOtherSdr,
    DeviceError,
    GainAuto,
    Muted,
    NoAudio,
    Step,
    Tune,
    Settings,
    Language,
    Theme,
    Gain,
    Audio,
    Dark,
    Light,
    On,
    Source,
    Back,
    Select,
    Menu,
    Move,
    Change,
    Count,
};

struct LocaleInfo {
    Locale locale;
    const char* code;
    const char* native_name;
    const char* font_asset;
};

constexpr size_t kLocaleCount = 10;

const std::array<LocaleInfo, kLocaleCount>& locales();
const LocaleInfo& locale_info(Locale locale);
const char* translate(Locale locale, Text text);

} // namespace i18n
