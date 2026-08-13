// SPDX-License-Identifier: MIT

#include "application_config.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::filesystem::path root(argv[1]);
    const auto config_path = root / "nested" / "cardputerzero-sdr.conf";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);

    app::ApplicationConfig written;
    written.dark_mode = true;
    written.locale = "zh-CN";
    written.frequency_hz = 103'900'000;
    written.tuning_step_index = 4;
    written.automatic_gain = false;
    written.gain_tenths_db = 190;
    written.muted = true;
    std::string error;
    assert(app::save_application_config(config_path.string(), written, error));
    assert(!std::filesystem::exists(config_path.string() + ".tmp"));

    app::ApplicationConfig loaded;
    assert(app::load_application_config(config_path.string(), loaded, error));
    assert(loaded.dark_mode);
    assert(loaded.locale == "zh-CN");
    assert(loaded.frequency_hz == 103'900'000);
    assert(loaded.tuning_step_index == 4);
    assert(!loaded.automatic_gain);
    assert(loaded.gain_tenths_db == 190);
    assert(loaded.muted);

    written.gain_tenths_db = -99;
    assert(app::save_application_config(config_path.string(), written, error));
    assert(app::load_application_config(config_path.string(), loaded, error));
    assert(loaded.gain_tenths_db == -99);

    {
        std::ofstream invalid(config_path, std::ios::trunc);
        invalid << "[application]\nlocale=../bad\n";
    }
    assert(!app::load_application_config(config_path.string(), loaded, error));
    assert(error.find("invalid locale") != std::string::npos);

    {
        std::ofstream invalid(config_path, std::ios::trunc);
        invalid << "[application]\nlocale=xx\n";
    }
    assert(!app::load_application_config(config_path.string(), loaded, error));
    assert(error.find("invalid locale") != std::string::npos);

    {
        std::ofstream invalid(config_path, std::ios::trunc);
        invalid << "[application]\nfrequency_hz=9999999999\n";
    }
    assert(!app::load_application_config(config_path.string(), loaded, error));
    assert(error.find("invalid frequency_hz") != std::string::npos);

    written.gain_tenths_db = 501;
    assert(!app::save_application_config(config_path.string(), written, error));
    assert(error.find("gain_tenths_db") != std::string::npos);

    std::filesystem::remove_all(root, ignored);
}
