/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */



#pragma once

#include <cstdint>

namespace view {

// some font constants
constexpr const char* ICON_SIGN_OUT           = "\uE42A";
constexpr const char* ICON_TEXT_BOLD          = "\uE5BE";
constexpr const char* ICON_MOON               = "\uE330";
constexpr const char* ICON_SUN                = "\uE474";
constexpr const char* ICON_SQUARE_ARROW_LEFT  = "\uE074";
constexpr const char* ICON_SQUARE_ARROW_RIGHT = "\uE076";
constexpr const char* ICON_MINUS              = "\uE32A";
constexpr const char* ICON_PLUS               = "\uE3D4";
constexpr const char* ICON_INFO               = "\uE2CE";
constexpr const char* ICON_WIFI_NONE          = "\uE4F0";
constexpr const char* ICON_WIFI_LOW           = "\uE4EC";
constexpr const char* ICON_WIFI_MEDIUM        = "\uE4EE";
constexpr const char* ICON_WIFI_HIGH          = "\uE4EA";
constexpr const char* ICON_ETHERNET           = "\uEDDE";
constexpr const char* ICON_BAT_FULL           = "\uE7C4";
constexpr const char* ICON_BAT_HIGH           = "\uE7C2";
constexpr const char* ICON_BAT_MEDIUM         = "\uE7C0";
constexpr const char* ICON_BAT_LOW            = "\uE7BE";
constexpr const char* ICON_BAT_EMPTY          = "\uE7C6";
constexpr const char* ICON_BAT_CHARGING       = "\uE0BC";

namespace color {

// Naive UI common color tokens, flattened to solid RGB values for LVGL.
namespace light {
constexpr uint32_t kPrimary      = 0x007f78;
constexpr uint32_t kInfo         = 0xc66a13;
constexpr uint32_t kBody         = 0xf5f8f9;
constexpr uint32_t kCard         = 0xffffff;
constexpr uint32_t kAction       = 0xe9f0f2;
constexpr uint32_t kButton       = 0xdfe9ec;
constexpr uint32_t kBorder       = 0xb8c9ce;
constexpr uint32_t kTextPrimary  = 0x10242b;
constexpr uint32_t kTextDisabled = 0x607981;
} // namespace light

namespace dark {
constexpr uint32_t kPrimary      = 0x35dcc8;
constexpr uint32_t kInfo         = 0xffaa45;
constexpr uint32_t kBody         = 0x03080d;
constexpr uint32_t kCard         = 0x06111b;
constexpr uint32_t kAction       = 0x08141d;
constexpr uint32_t kButton       = 0x0c1b26;
constexpr uint32_t kBorder       = 0x173143;
constexpr uint32_t kTextPrimary  = 0xeaf7f7;
constexpr uint32_t kTextDisabled = 0x78909a;
} // namespace dark

} // namespace color

} // namespace view
