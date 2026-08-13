/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace app {

struct ApplicationConfig {
    bool dark_mode = false;
    std::string locale = "en";
    uint32_t frequency_hz = 97'400'000;
    size_t tuning_step_index = 3;
    bool automatic_gain = true;
    int gain_tenths_db = 200;
    bool muted = false;
};

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error);
bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error);

} // namespace app
