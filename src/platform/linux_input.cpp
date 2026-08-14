#include "linux_input.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#if !USE_DESKTOP
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#ifndef APP_KEY_INPUT_DEVICE
#define APP_KEY_INPUT_DEVICE ""
#endif

#ifndef APP_CARDPUTER_KEY_INPUT_DEVICE
// Stable TCA8418 path used by the official Cardputer Zero APPLaunch runtime.
#define APP_CARDPUTER_KEY_INPUT_DEVICE "/dev/input/by-path/platform-3f804000.i2c-event"
#endif

namespace platform {
namespace {

std::array<lv_obj_t*, kNavKeyCount> nav_buttons{};
uint32_t last_key = 0;
bool last_key_pressed = false;
bool nav_shortcut_mode = false;
AppKeyHandler app_key_handler = nullptr;
void* app_key_user_data = nullptr;
AppInputActivity input_activity{};

#if !USE_DESKTOP
struct EvdevKeypad {
    int fd{-1};
    lv_indev_state_t state{LV_INDEV_STATE_RELEASED};
    uint32_t key{0};
    bool router_event_pending{false};
    uint32_t router_key{0};
    bool router_pressed{false};
    bool router_repeated{false};
    bool synthesize_repeat{true};
    AppKeyRepeatState repeat{};
};
#endif

void dispatch_nav_key(uint32_t key);

AppKey normalize_app_key(uint32_t key) {
    switch (key) {
        case LV_KEY_UP:
        case 'f':
        case 'F':
            return AppKey::Up;
        case LV_KEY_DOWN:
        case 'x':
        case 'X':
            return AppKey::Down;
        case LV_KEY_LEFT:
        case 'z':
        case 'Z':
            return AppKey::Left;
        case LV_KEY_RIGHT:
        case 'c':
        case 'C':
            return AppKey::Right;
        case LV_KEY_ENTER:
            return AppKey::Confirm;
        case LV_KEY_ESC:
            return AppKey::Back;
        case 'g':
        case 'G':
            return AppKey::Gain;
        case 'm':
        case 'M':
            return AppKey::Mute;
        case 'l':
        case 'L':
            return AppKey::Language;
        case 't':
        case 'T':
            return AppKey::Theme;
        case '0': return AppKey::Digit0;
        case '1': return AppKey::Digit1;
        case '2': return AppKey::Digit2;
        case '3': return AppKey::Digit3;
        case '4': return AppKey::Digit4;
        case '5': return AppKey::Digit5;
        case '6': return AppKey::Digit6;
        case '7': return AppKey::Digit7;
        case '8': return AppKey::Digit8;
        case '9': return AppKey::Digit9;
        case '.': return AppKey::Decimal;
        case LV_KEY_BACKSPACE:
        case LV_KEY_DEL:
            return AppKey::Delete;
        default:
            return AppKey::None;
    }
}

void dispatch_app_key(uint32_t key, bool repeated) {
    if (!app_key_handler) {
        dispatch_nav_key(key);
        return;
    }

    const auto app_key = normalize_app_key(key);
    if (app_key != AppKey::None) {
        input_activity.record(app_key, repeated);
        app_key_handler(app_key, repeated, app_key_user_data);
    }
}

size_t nav_key_to_index(uint32_t key) {
    if (nav_shortcut_mode) {
        switch (key) {
            case LV_KEY_ESC:
                return 4;
            case 'z':
            case 'Z':
            case LV_KEY_LEFT:
                return 1;
            case 'c':
            case 'C':
            case LV_KEY_RIGHT:
                return 3;
            default:
                return kNavKeyCount;
        }
    }

    switch (key) {
        case '4':
        case LV_KEY_ESC:
            return 0;
        case '5':
            return 1;
        case '6':
            return 2;
        case '7':
            return 3;
        case '8':
            return 4;
        default:
            return kNavKeyCount;
    }
}

void dispatch_nav_key(uint32_t key) {
    const auto index = nav_key_to_index(key);
    if (index >= nav_buttons.size()) {
        return;
    }

    auto* button = nav_buttons[index];
    if (!button || !lv_obj_is_valid(button) || !lv_obj_has_flag(button, LV_OBJ_FLAG_CLICKABLE)) {
        return;
    }

    lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
}

void key_event_cb(lv_event_t* event) {
    LV_UNUSED(event);

    auto* indev = lv_indev_active();
    if (!indev) {
        return;
    }

#if !USE_DESKTOP
    auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
    if (keypad) {
        // LVGL emits LV_EVENT_KEY on every poll, even when evdev had no new event.
        if (!keypad->router_event_pending) {
            return;
        }

        keypad->router_event_pending = false;
        const auto key = keypad->router_key;
        const bool pressed = keypad->router_pressed;
        const bool repeated = keypad->router_repeated;

        if (pressed && (repeated || !last_key_pressed || last_key != key)) {
            dispatch_app_key(key, repeated);
        }

        last_key = key;
        last_key_pressed = pressed;
        return;
    }
#endif

    const auto key = lv_indev_get_key(indev);
    const bool pressed = lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;

    if (pressed && (!last_key_pressed || last_key != key)) {
        dispatch_app_key(key, false);
    }

    last_key = key;
    last_key_pressed = pressed;
}

#if !USE_DESKTOP
uint32_t map_evdev_key(uint16_t code) {
    switch (code) {
        case KEY_ESC:
            return LV_KEY_ESC;
        case KEY_LEFT:
            return LV_KEY_LEFT;
        case KEY_RIGHT:
            return LV_KEY_RIGHT;
        case KEY_UP:
            return LV_KEY_UP;
        case KEY_DOWN:
            return LV_KEY_DOWN;
        case KEY_ENTER:
        case KEY_KPENTER:
            return LV_KEY_ENTER;
        case KEY_Z:
            return 'z';
        case KEY_X:
            return 'x';
        case KEY_C:
            return 'c';
        case KEY_F:
            return 'f';
        case KEY_G:
            return 'g';
        case KEY_M:
            return 'm';
        case KEY_L:
            return 'l';
        case KEY_T:
            return 't';
        case KEY_0:
            return '0';
        case KEY_1:
            return '1';
        case KEY_2:
            return '2';
        case KEY_3:
            return '3';
        case KEY_4:
            return '4';
        case KEY_5:
            return '5';
        case KEY_6:
            return '6';
        case KEY_7:
            return '7';
        case KEY_8:
            return '8';
        case KEY_9:
            return '9';
        case KEY_DOT:
        case KEY_KPDOT:
            return '.';
        case KEY_BACKSPACE:
        case KEY_DELETE:
            return LV_KEY_BACKSPACE;
        default:
            return 0;
    }
}

bool has_app_keys(int fd, bool& kernel_repeat_enabled) {
    unsigned long event_bits[(EV_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    kernel_repeat_enabled = false;
    if (ioctl(fd, EVIOCGBIT(0, sizeof(event_bits)), event_bits) >= 0) {
        const auto bits_per_word = static_cast<int>(sizeof(unsigned long) * 8);
        kernel_repeat_enabled =
            (event_bits[EV_REP / bits_per_word] & (1UL << (EV_REP % bits_per_word))) != 0;
    }

    unsigned long key_bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return false;
    }

    auto has_key = [&](int code) {
        const auto bits_per_word = static_cast<int>(sizeof(unsigned long) * 8);
        return (key_bits[code / bits_per_word] & (1UL << (code % bits_per_word))) != 0;
    };

    uint32_t capabilities = 0;
    const bool directional_keys =
        (has_key(KEY_F) && has_key(KEY_X) && has_key(KEY_Z) && has_key(KEY_C)) ||
        (has_key(KEY_UP) && has_key(KEY_DOWN) && has_key(KEY_LEFT) && has_key(KEY_RIGHT));
    if (directional_keys) {
        capabilities |= app_keyboard_capability(AppKeyboardCapability::Navigation);
    }
    if (has_key(KEY_ENTER) && has_key(KEY_ESC)) {
        capabilities |= app_keyboard_capability(AppKeyboardCapability::ConfirmAndBack);
    }
    if (has_key(KEY_G) && has_key(KEY_M) && has_key(KEY_L) && has_key(KEY_T)) {
        capabilities |= app_keyboard_capability(AppKeyboardCapability::RadioShortcuts);
    }
    const bool number_row = has_key(KEY_0) && has_key(KEY_1) && has_key(KEY_2) &&
                            has_key(KEY_3) && has_key(KEY_4) && has_key(KEY_5) &&
                            has_key(KEY_6) && has_key(KEY_7) && has_key(KEY_8) &&
                            has_key(KEY_9);
    if (number_row && (has_key(KEY_DOT) || has_key(KEY_KPDOT)) &&
        (has_key(KEY_BACKSPACE) || has_key(KEY_DELETE))) {
        capabilities |= app_keyboard_capability(AppKeyboardCapability::DirectEntry);
    }
    return supports_zero_sdr_controls(capabilities);
}

void evdev_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
    data->continue_reading = false;
    if (!keypad) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    bool physical_event = false;
    input_event input{};
    while (read(keypad->fd, &input, sizeof(input)) == sizeof(input)) {
        if (input.type != EV_KEY) {
            continue;
        }

        const auto key = map_evdev_key(input.code);
        if (!key) {
            continue;
        }

        keypad->key = key;
        keypad->state = input.value == 0 ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
        keypad->router_event_pending = true;
        keypad->router_key = key;
        keypad->router_pressed = keypad->state == LV_INDEV_STATE_PRESSED;
        keypad->router_repeated = input.value == 2;
        const uint32_t now = lv_tick_get();
        if (input.value == 0) {
            keypad->repeat.release(key);
        } else if (input.value == 1 && keypad->synthesize_repeat) {
            keypad->repeat.press(key, now);
        } else if (input.value == 2 && keypad->synthesize_repeat) {
            keypad->repeat.observed_repeat(key, now);
        }
        physical_event = true;
        data->continue_reading = true;
        break;
    }

    if (!physical_event && keypad->repeat.armed) {
        const uint32_t now = lv_tick_get();
        if (keypad->repeat.due(now)) {
            keypad->key = keypad->repeat.key;
            keypad->state = LV_INDEV_STATE_PRESSED;
            keypad->router_event_pending = true;
            keypad->router_key = keypad->repeat.key;
            keypad->router_pressed = true;
            keypad->router_repeated = true;
            keypad->repeat.emitted(now);
        }
    }

    data->key = keypad->key;
    data->state = keypad->state;
}

void evdev_delete_cb(lv_event_t* event) {
    auto* indev = static_cast<lv_indev_t*>(lv_event_get_target(event));
    auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
    if (!keypad) {
        return;
    }

    if (keypad->fd >= 0) {
        close(keypad->fd);
    }
    delete keypad;
}

lv_indev_t* create_keypad_from_fd(int fd, bool kernel_repeat_enabled) {
    auto* keypad = new EvdevKeypad;
    keypad->fd = fd;
    keypad->synthesize_repeat = !kernel_repeat_enabled;

    auto* indev = lv_indev_create();
    if (!indev) {
        delete keypad;
        close(fd);
        return nullptr;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, evdev_read_cb);
    lv_indev_set_driver_data(indev, keypad);
    lv_indev_add_event_cb(indev, evdev_delete_cb, LV_EVENT_DELETE, nullptr);
    attach_key_router(indev);
    return indev;
}

lv_indev_t* try_create_keypad(const char* path) {
    if (!path || path[0] == '\0') {
        return nullptr;
    }

    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LV_LOG_WARN("failed to open input device %s: %s", path, strerror(errno));
        return nullptr;
    }

    bool kernel_repeat_enabled = false;
    if (!has_app_keys(fd, kernel_repeat_enabled)) {
        close(fd);
        return nullptr;
    }

    LV_LOG_INFO("using evdev key input %s (%s repeat)", path,
                kernel_repeat_enabled ? "kernel" : "userspace");
    return create_keypad_from_fd(fd, kernel_repeat_enabled);
}

void discover_keypads(lv_display_t* display) {
    const char* configured_device = APP_KEY_INPUT_DEVICE;
    if (configured_device[0] != '\0') {
        auto* indev = try_create_keypad(configured_device);
        if (indev) {
            lv_indev_set_display(indev, display);
        }
        return;
    }

    const std::array<const char*, 3> preferred_devices = {{
        std::getenv("LV_LINUX_KEYBOARD_DEVICE"),
        std::getenv("APPLAUNCH_LINUX_KEYBOARD_DEVICE"),
        APP_CARDPUTER_KEY_INPUT_DEVICE,
    }};
    for (size_t index = 0; index < preferred_devices.size(); ++index) {
        const char* preferred_device = preferred_devices[index];
        if (!preferred_device || preferred_device[0] == '\0') continue;
        bool already_tried = false;
        for (size_t earlier = 0; earlier < index; ++earlier) {
            const char* earlier_device = preferred_devices[earlier];
            if (earlier_device && std::strcmp(preferred_device, earlier_device) == 0) {
                already_tried = true;
                break;
            }
        }
        if (already_tried) continue;
        auto* indev = try_create_keypad(preferred_device);
        if (indev) {
            lv_indev_set_display(indev, display);
            return;
        }
    }

    auto* dir = opendir("/dev/input");
    if (!dir) {
        LV_LOG_WARN("failed to open /dev/input: %s", strerror(errno));
        return;
    }

    while (auto* entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        std::string path = "/dev/input/";
        path += entry->d_name;
        auto* indev = try_create_keypad(path.c_str());
        if (indev) {
            lv_indev_set_display(indev, display);
            closedir(dir);
            return;
        }
    }

    closedir(dir);
}
#endif

} // namespace

void init_key_input(lv_display_t* display) {
#if !USE_DESKTOP
    discover_keypads(display);
#else
    LV_UNUSED(display);
#endif
}

int app_key_digit(AppKey key) {
    const auto value = static_cast<int>(key) - static_cast<int>(AppKey::Digit0);
    return value >= 0 && value <= 9 ? value : -1;
}

void attach_key_router(lv_indev_t* indev) {
    if (!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_KEYPAD) {
        return;
    }

    lv_indev_add_event_cb(indev, key_event_cb, LV_EVENT_KEY, nullptr);
}

void set_app_key_handler(AppKeyHandler handler, void* user_data) {
    app_key_handler = handler;
    app_key_user_data = user_data;
    last_key = 0;
    last_key_pressed = false;
}

void clear_app_key_handler(void* user_data) {
    if (!user_data || app_key_user_data == user_data) {
        app_key_handler = nullptr;
        app_key_user_data = nullptr;
        last_key = 0;
        last_key_pressed = false;
    }
}

AppInputActivity app_input_activity_metrics() {
    return input_activity;
}

void reset_app_input_activity_metrics() {
    input_activity = {};
}

void set_nav_shortcut_mode(bool enabled) {
    nav_shortcut_mode = enabled;
}

void register_nav_button(size_t index, lv_obj_t* button) {
    if (index >= nav_buttons.size()) {
        return;
    }

    nav_buttons[index] = button;
}

void unregister_nav_button(size_t index, lv_obj_t* button) {
    if (index >= nav_buttons.size()) {
        return;
    }

    if (!button || nav_buttons[index] == button) {
        nav_buttons[index] = nullptr;
    }
}

} // namespace platform
