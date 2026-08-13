/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

#include "application_config.h"
#include "asset_manager.h"
#include "logger.h"
#include "linux_input.h"
#include "screen_manager.h"
#include "base_viewmodel.h"
#include "theme.h"

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>

#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#endif

#if !USE_DESKTOP
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

#ifndef APP_CONFIG_FILE
#define APP_CONFIG_FILE "cardputerzero-sdr.conf"
#endif

namespace app {
namespace {

volatile std::sig_atomic_t termination_requested = 0;

void termination_signal_handler(int) {
    termination_requested = 1;
}

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

struct SettingsPersistence {
    std::string config_path;
    viewmodel::BaseViewModel* view_model;
    ApplicationConfig last_saved;
};

ApplicationConfig current_config(const viewmodel::BaseViewModel& view_model) {
    ApplicationConfig config;
    config.dark_mode = view_model.is_dark_mode();
    config.locale = i18n::locale_info(view_model.locale()).code;
    config.frequency_hz = view_model.frequency_hz();
    config.tuning_step_index = view_model.tuning_step_index();
    config.automatic_gain = view_model.automatic_gain();
    config.gain_tenths_db = view_model.gain_tenths_db();
    config.muted = view_model.is_muted();
    return config;
}

bool same_config(const ApplicationConfig& left, const ApplicationConfig& right) {
    return left.dark_mode == right.dark_mode &&
           left.locale == right.locale &&
           left.frequency_hz == right.frequency_hz &&
           left.tuning_step_index == right.tuning_step_index &&
           left.automatic_gain == right.automatic_gain &&
           left.gain_tenths_db == right.gain_tenths_db &&
           left.muted == right.muted;
}

std::string writable_config_path() {
    if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
        const std::filesystem::path root(xdg_config_home);
        if (!root.empty() && root.is_absolute()) {
            return (root / "cardputerzero-sdr" / "cardputerzero-sdr.conf").string();
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path root(home);
        if (!root.empty() && root.is_absolute()) {
            return (root / ".config" / "cardputerzero-sdr" / "cardputerzero-sdr.conf").string();
        }
    }
    return APP_CONFIG_FILE;
}

uint32_t diagnostics_interval_ms() {
    constexpr uint32_t kDefaultIntervalMs = 30'000;
    const char* value = std::getenv("ZERO_SDR_DIAGNOSTICS_INTERVAL_MS");
    if (!value || value[0] == '\0') return kDefaultIntervalMs;

    errno = 0;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed < 100 ||
        parsed > 3'600'000 || parsed > std::numeric_limits<uint32_t>::max()) {
        LOG_WARN("invalid ZERO_SDR_DIAGNOSTICS_INTERVAL_MS={}; using {}", value,
                 kDefaultIntervalMs);
        return kDefaultIntervalMs;
    }
    return static_cast<uint32_t>(parsed);
}

void persist_settings_observer(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);
    auto* persistence = static_cast<SettingsPersistence*>(lv_observer_get_user_data(observer));
    if (!persistence) {
        return;
    }
    const auto config = current_config(*persistence->view_model);
    if (same_config(config, persistence->last_saved)) return;

    std::string error;
    if (save_application_config(persistence->config_path, config, error)) {
        persistence->last_saved = config;
        LOG_INFO("saved config: {} (dark_mode={}, locale={}, frequency_hz={}, step={}, auto_gain={}, gain={}, muted={})",
                 persistence->config_path,
                 config.dark_mode ? "yes" : "no",
                 config.locale,
                 config.frequency_hz,
                 config.tuning_step_index,
                 config.automatic_gain ? "yes" : "no",
                 config.gain_tenths_db,
                 config.muted ? "yes" : "no");
    } else {
        LOG_WARN("failed to save config {}: {}", persistence->config_path, error);
    }
}

#if !USE_DESKTOP
lv_display_t* init_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#endif
}
#endif

} // namespace

int Application::run() {
    logger::Logger::init();
    logger::Logger::set_tag("zero-sdr");
    logger::Logger::set_timestamp_enabled(true);

    lv_init();

    AssetManager assets;
    for (const auto& root : assets.roots()) {
        LOG_INFO("asset root: {}", root.string());
    }

    viewmodel::BaseViewModel view_model;
    const std::string user_config_path = writable_config_path();
    std::string loaded_config_path = APP_CONFIG_FILE;
    if (user_config_path != APP_CONFIG_FILE) {
        std::error_code filesystem_error;
        if (std::filesystem::is_regular_file(user_config_path, filesystem_error)) {
            loaded_config_path = user_config_path;
        }
    }

    ApplicationConfig config;
    std::string config_error;
    if (load_application_config(loaded_config_path, config, config_error)) {
        view_model.set_dark_mode(config.dark_mode);
        for (const auto& locale : i18n::locales()) {
            if (config.locale == locale.code) {
                view_model.set_locale(locale.locale);
                break;
            }
        }
        view_model.restore_radio_settings(config.frequency_hz,
                                          config.tuning_step_index,
                                          config.automatic_gain,
                                          config.gain_tenths_db,
                                          config.muted);
        LOG_INFO("loaded config: {} (dark_mode={}, locale={}, frequency_hz={}, step={}, auto_gain={}, gain={}, muted={})",
                 loaded_config_path,
                 config.dark_mode ? "yes" : "no",
                 config.locale,
                 config.frequency_hz,
                 config.tuning_step_index,
                 config.automatic_gain ? "yes" : "no",
                 config.gain_tenths_db,
                 config.muted ? "yes" : "no");
    } else {
        LOG_WARN("failed to load config {}: {}; using defaults", loaded_config_path, config_error);
    }

#if USE_DESKTOP
    if (const char* locale_code = std::getenv("ZERO_SDR_LOCALE")) {
        for (const auto& locale : i18n::locales()) {
            if (std::string(locale.code) == locale_code) {
                view_model.set_locale(locale.locale);
                break;
            }
        }
    }
    if (const char* start_page = std::getenv("ZERO_SDR_START_PAGE")) {
        if (std::string(start_page) == "settings") {
            view_model.show_settings_page();
        }
    }
#endif

#if USE_DESKTOP
    DesktopSimulatorFrame simulator_frame(assets);
    auto* display = simulator_frame.display();
#else
    auto* display = init_device_display();
#endif
    if (!display) {
        LOG_ERROR("failed to initialize display");
        return 1;
    }

    view::apply_lvgl_theme(display, view_model.is_dark_mode());

#if USE_DESKTOP
    simulator_frame.bind_dark_mode(view_model.dark_mode_subject());
#endif

    ScreenManager screen_manager(view_model, assets);
    screen_manager.start();

    SettingsPersistence settings_persistence{
        user_config_path,
        &view_model,
        current_config(view_model),
    };
    const std::array<lv_observer_t*, 6> settings_observers = {{
        lv_subject_add_observer(view_model.dark_mode_subject(), persist_settings_observer, &settings_persistence),
        lv_subject_add_observer(view_model.locale_subject(), persist_settings_observer, &settings_persistence),
        lv_subject_add_observer(view_model.frequency_subject(), persist_settings_observer, &settings_persistence),
        lv_subject_add_observer(view_model.step_subject(), persist_settings_observer, &settings_persistence),
        lv_subject_add_observer(view_model.gain_subject(), persist_settings_observer, &settings_persistence),
        lv_subject_add_observer(view_model.muted_subject(), persist_settings_observer, &settings_persistence),
    }};

    bool running = true;
    auto* quit_observer = lv_subject_add_observer(view_model.quit_requested_subject(),
                                                  quit_requested_observer,
                                                  &running);

    termination_requested = 0;
    const auto sigint_result = std::signal(SIGINT, termination_signal_handler);
    const auto sigterm_result = std::signal(SIGTERM, termination_signal_handler);
    if (sigint_result == SIG_ERR || sigterm_result == SIG_ERR) {
        LOG_WARN("unable to install one or more termination signal handlers");
    }

    LOG_INFO("LVGL app started at {}x{}", lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));
    const uint32_t diagnostics_interval = diagnostics_interval_ms();
    const uint32_t diagnostics_started_at = lv_tick_get();
    uint32_t diagnostics_last_at = diagnostics_started_at;
    uint32_t ui_loop_last_at = diagnostics_started_at;
    uint32_t ui_loop_max_gap_ms = 0;
    uint64_t ui_loop_count = 0;
#if USE_DESKTOP
    const char* screenshot_path = std::getenv("ZERO_SDR_SCREENSHOT");
    const bool exit_after_screenshot = std::getenv("ZERO_SDR_SCREENSHOT_EXIT") != nullptr;
    const uint32_t screenshot_started_at = lv_tick_get();
    bool screenshot_saved = false;
#endif
    while (running
#if USE_DESKTOP
           && simulator_frame.process_events()
#endif
           && !termination_requested
    ) {
        view_model.poll_radio_session();
        const uint32_t now = lv_tick_get();
        const uint32_t ui_loop_gap_ms = lv_tick_elaps(ui_loop_last_at);
        if (ui_loop_gap_ms > ui_loop_max_gap_ms) ui_loop_max_gap_ms = ui_loop_gap_ms;
        ui_loop_last_at = now;
        ++ui_loop_count;
        if (lv_tick_elaps(diagnostics_last_at) >= diagnostics_interval) {
            const auto metrics = view_model.radio_metrics();
            LOG_INFO("diagnostics uptime_ms={} frequency_hz={} muted={} "
                     "ui_loops={} max_ui_gap_ms={} "
                     "connection_attempts={} successful_connections={} retry_waits={} "
                     "read_errors={} settings_updates={} iq_blocks={} iq_bytes={} "
                     "audio_generated={} audio_written={} audio_dropped={} "
                     "audio_recoveries={} audio_write_errors={} audio_open_failures={} "
                     "total_processing_us={} max_processing_us={}",
                     lv_tick_elaps(diagnostics_started_at),
                     view_model.frequency_hz(),
                     view_model.is_muted() ? 1 : 0,
                     ui_loop_count,
                     ui_loop_max_gap_ms,
                     metrics.connection_attempts,
                     metrics.successful_connections,
                     metrics.retry_waits,
                     metrics.read_errors,
                     metrics.settings_updates,
                     metrics.iq_blocks,
                     metrics.iq_bytes,
                     metrics.audio_frames_generated,
                     metrics.audio_frames_written,
                     metrics.audio_frames_dropped,
                     metrics.audio_recoveries,
                     metrics.audio_write_errors,
                     metrics.audio_open_failures,
                     metrics.total_processing_us,
                     metrics.maximum_processing_us);
            diagnostics_last_at = now;
        }
        lv_timer_handler();
#if USE_DESKTOP
        if (!screenshot_saved && screenshot_path && lv_tick_elaps(screenshot_started_at) >= 600U) {
            screenshot_saved = simulator_frame.save_screen_png(screenshot_path);
            if (screenshot_saved && exit_after_screenshot) {
                running = false;
            }
        }
#endif
        lv_delay_ms(5);
    }

    if (termination_requested) {
        LOG_INFO("termination signal received; shutting down cleanly");
    }

    if (quit_observer) {
        lv_observer_remove(quit_observer);
    }
    for (auto* observer : settings_observers) {
        if (observer) lv_observer_remove(observer);
    }

    return 0;
}

} // namespace app
