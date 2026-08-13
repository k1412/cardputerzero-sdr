/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "application_config.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <vector>

namespace app {
namespace {

std::string trim(std::string value) {
    const auto is_not_space = [](unsigned char character) {
        return !std::isspace(character);
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), is_not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), is_not_space).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool parse_bool(const std::string& value, bool& result) {
    const auto normalized = lowercase(trim(value));
    if (normalized == "yes" || normalized == "true" || normalized == "1") {
        result = true;
        return true;
    }
    if (normalized == "no" || normalized == "false" || normalized == "0") {
        result = false;
        return true;
    }
    return false;
}

template <typename Integer>
bool parse_integer(const std::string& value, Integer& result) {
    const auto normalized = trim(value);
    if (normalized.empty()) return false;
    Integer parsed{};
    const auto conversion = std::from_chars(
        normalized.data(), normalized.data() + normalized.size(), parsed);
    if (conversion.ec != std::errc{} ||
        conversion.ptr != normalized.data() + normalized.size()) {
        return false;
    }
    result = parsed;
    return true;
}

bool parse_locale(const std::string& value, std::string& result) {
    const auto normalized = trim(value);
    if (normalized.empty() || normalized.size() > 16) return false;
    const bool valid = std::all_of(normalized.begin(), normalized.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '-';
    });
    if (!valid) return false;
    constexpr std::array<std::string_view, 10> supported = {
        "en", "zh-CN", "zh-TW", "es", "ja", "ko", "fr", "de", "pt-BR", "ru",
    };
    if (std::find(supported.begin(), supported.end(), normalized) == supported.end()) {
        return false;
    }
    result = normalized;
    return true;
}

bool validate_config(const ApplicationConfig& config, std::string& error) {
    std::string locale;
    if (!parse_locale(config.locale, locale)) {
        error = "invalid locale value";
        return false;
    }
    if (config.frequency_hz < 22'000'000 || config.frequency_hz > 948'600'000) {
        error = "frequency_hz is outside the supported tuner range";
        return false;
    }
    if (config.tuning_step_index >= 6) {
        error = "tuning_step_index is outside the supported range";
        return false;
    }
    if (config.gain_tenths_db < ApplicationConfig::kMinimumGainTenthsDb ||
        config.gain_tenths_db > ApplicationConfig::kMaximumGainTenthsDb) {
        error = "gain_tenths_db is outside the supported range";
        return false;
    }
    return true;
}

bool is_section(const std::string& line, const std::string& expected) {
    const auto normalized = trim(line);
    return normalized.size() >= 2 && normalized.front() == '[' && normalized.back() == ']' &&
           lowercase(trim(normalized.substr(1, normalized.size() - 2))) == expected;
}

bool is_key(const std::string& line, const std::string& expected) {
    const auto separator = line.find('=');
    return separator != std::string::npos &&
           lowercase(trim(line.substr(0, separator))) == expected;
}

} // namespace

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "could not open file";
        return false;
    }

    std::string section;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = lowercase(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = lowercase(trim(line.substr(0, separator)));
        if (section == "application" && key == "dark_mode") {
            if (!parse_bool(line.substr(separator + 1), config.dark_mode)) {
                error = "invalid dark_mode value at line " + std::to_string(line_number);
                return false;
            }
        }
        else if (section == "application" && key == "locale") {
            if (!parse_locale(line.substr(separator + 1), config.locale)) {
                error = "invalid locale value at line " + std::to_string(line_number);
                return false;
            }
        }
        else if (section == "application" && key == "frequency_hz") {
            if (!parse_integer(line.substr(separator + 1), config.frequency_hz) ||
                config.frequency_hz < 22'000'000 || config.frequency_hz > 948'600'000) {
                error = "invalid frequency_hz value at line " + std::to_string(line_number);
                return false;
            }
        }
        else if (section == "application" && key == "tuning_step_index") {
            if (!parse_integer(line.substr(separator + 1), config.tuning_step_index) ||
                config.tuning_step_index >= 6) {
                error = "invalid tuning_step_index value at line " + std::to_string(line_number);
                return false;
            }
        }
        else if (section == "application" && key == "automatic_gain") {
            if (!parse_bool(line.substr(separator + 1), config.automatic_gain)) {
                error = "invalid automatic_gain value at line " + std::to_string(line_number);
                return false;
            }
        }
        else if (section == "application" && key == "gain_tenths_db") {
            if (!parse_integer(line.substr(separator + 1), config.gain_tenths_db) ||
                config.gain_tenths_db < ApplicationConfig::kMinimumGainTenthsDb ||
                config.gain_tenths_db > ApplicationConfig::kMaximumGainTenthsDb) {
                error = "invalid gain_tenths_db value at line " + std::to_string(line_number);
                return false;
            }
        }
        else if (section == "application" && key == "muted") {
            if (!parse_bool(line.substr(separator + 1), config.muted)) {
                error = "invalid muted value at line " + std::to_string(line_number);
                return false;
            }
        }
    }

    return true;
}

bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error) {
    error.clear();
    if (!validate_config(config, error)) return false;

    const std::filesystem::path config_path(path);
    const auto parent = config_path.parent_path();
    if (!parent.empty()) {
        std::error_code filesystem_error;
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) {
            error = "could not create parent directory: " + filesystem_error.message();
            return false;
        }
    }

    std::vector<std::string> lines;
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }

    std::vector<std::string> updated;
    bool in_application_section = false;
    bool found_application_section = false;
    struct Entry {
        const char* key;
        std::string line;
        bool written{false};
    };
    std::array<Entry, 7> entries = {{
        {"dark_mode", config.dark_mode ? "dark_mode=yes" : "dark_mode=no"},
        {"locale", "locale=" + config.locale},
        {"frequency_hz", "frequency_hz=" + std::to_string(config.frequency_hz)},
        {"tuning_step_index", "tuning_step_index=" + std::to_string(config.tuning_step_index)},
        {"automatic_gain", config.automatic_gain ? "automatic_gain=yes" : "automatic_gain=no"},
        {"gain_tenths_db", "gain_tenths_db=" + std::to_string(config.gain_tenths_db)},
        {"muted", config.muted ? "muted=yes" : "muted=no"},
    }};

    const auto append_missing = [&updated, &entries] {
        for (auto& entry : entries) {
            if (!entry.written) {
                updated.push_back(entry.line);
                entry.written = true;
            }
        }
    };

    for (const auto& current_line : lines) {
        const auto normalized = trim(current_line);
        const bool section_header = normalized.size() >= 2 && normalized.front() == '[' &&
                                    normalized.back() == ']';
        if (section_header) {
            if (in_application_section) append_missing();
            in_application_section = is_section(current_line, "application");
            found_application_section = found_application_section || in_application_section;
        }

        bool replaced = false;
        if (in_application_section) {
            for (auto& entry : entries) {
                if (is_key(current_line, entry.key)) {
                    if (!entry.written) {
                        updated.push_back(entry.line);
                        entry.written = true;
                    }
                    replaced = true;
                    break;
                }
            }
        }
        if (replaced) continue;
        updated.push_back(current_line);
    }

    if (!found_application_section) {
        if (!updated.empty() && !updated.back().empty()) {
            updated.emplace_back();
        }
        updated.emplace_back("[application]");
    }
    append_missing();

    auto temporary_path = config_path;
    temporary_path += ".tmp";
    std::ofstream output(temporary_path, std::ios::trunc);
    if (!output) {
        error = "could not open file for writing";
        return false;
    }
    for (const auto& updated_line : updated) {
        output << updated_line << '\n';
    }
    if (!output) {
        error = "failed while writing file";
        return false;
    }
    output.close();

#if defined(_WIN32)
    std::error_code remove_error;
    std::filesystem::remove(config_path, remove_error);
#endif
    std::error_code rename_error;
    std::filesystem::rename(temporary_path, config_path, rename_error);
    if (rename_error) {
        error = "could not replace config file: " + rename_error.message();
        return false;
    }

    return true;
}

} // namespace app
