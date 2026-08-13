// SPDX-License-Identifier: MIT

#pragma once

#include <string>

namespace platform {

// Resolve the framebuffer in the same order as the official APPLaunch runtime:
// an explicit LVGL override, the path exported to child applications, then the
// build-time default.
std::string resolve_framebuffer_device(const char* lv_linux_device,
                                       const char* applaunch_device,
                                       const char* compiled_default);

std::string framebuffer_device();

} // namespace platform
