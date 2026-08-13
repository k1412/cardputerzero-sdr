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
    std::string error;
    assert(app::save_application_config(config_path.string(), written, error));

    app::ApplicationConfig loaded;
    assert(app::load_application_config(config_path.string(), loaded, error));
    assert(loaded.dark_mode);
    assert(loaded.locale == "zh-CN");

    {
        std::ofstream invalid(config_path, std::ios::trunc);
        invalid << "[application]\nlocale=../bad\n";
    }
    assert(!app::load_application_config(config_path.string(), loaded, error));
    assert(error.find("invalid locale") != std::string::npos);

    std::filesystem::remove_all(root, ignored);
}
