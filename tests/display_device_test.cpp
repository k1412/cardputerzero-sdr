// SPDX-License-Identifier: MIT

#include "display_device.h"

#include <cassert>

int main() {
    using platform::resolve_framebuffer_device;

    assert(resolve_framebuffer_device(nullptr, nullptr, "/dev/fb3") == "/dev/fb3");
    assert(resolve_framebuffer_device("", "", "/dev/fb3") == "/dev/fb3");
    assert(resolve_framebuffer_device(nullptr, "/dev/fb2", "/dev/fb3") == "/dev/fb2");
    assert(resolve_framebuffer_device("", "/dev/fb2", "/dev/fb3") == "/dev/fb2");
    assert(resolve_framebuffer_device("/dev/fb1", "/dev/fb2", "/dev/fb3") == "/dev/fb1");
    assert(resolve_framebuffer_device(nullptr, nullptr, nullptr) == "/dev/fb0");
    assert(resolve_framebuffer_device(nullptr, nullptr, "") == "/dev/fb0");
}
