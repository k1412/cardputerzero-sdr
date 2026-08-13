// SPDX-License-Identifier: MIT

#include "display_device.h"

#include <cstdlib>

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

namespace platform {
namespace {

const char* nonempty(const char* value) {
    return value && value[0] != '\0' ? value : nullptr;
}

} // namespace

std::string resolve_framebuffer_device(const char* lv_linux_device,
                                       const char* applaunch_device,
                                       const char* compiled_default) {
    if (const char* value = nonempty(lv_linux_device)) {
        return value;
    }
    if (const char* value = nonempty(applaunch_device)) {
        return value;
    }
    if (const char* value = nonempty(compiled_default)) {
        return value;
    }
    return "/dev/fb0";
}

std::string framebuffer_device() {
    return resolve_framebuffer_device(std::getenv("LV_LINUX_FBDEV_DEVICE"),
                                      std::getenv("APPLAUNCH_LINUX_FBDEV_DEVICE"),
                                      APP_FRAMEBUFFER_DEVICE);
}

} // namespace platform
