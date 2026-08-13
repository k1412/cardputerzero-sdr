// SPDX-License-Identifier: MIT

#include "device_status.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

void write(const std::filesystem::path& path, const std::string& value) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    assert(output);
    output << value;
    assert(output.good());
}

void write_battery(const std::filesystem::path& root,
                   const std::string& name,
                   int present,
                   int capacity,
                   const std::string& state) {
    const auto battery = root / "power" / name;
    write(battery / "type", "Battery\n");
    write(battery / "present", std::to_string(present) + "\n");
    write(battery / "capacity", std::to_string(capacity) + "\n");
    write(battery / "status", state + "\n");
}

} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::filesystem::path root(argv[1]);
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    auto status = platform::read_device_status();
    assert(!status.battery_present);
    assert(!status.wifi_present);
    assert(!status.ethernet_present);

    // Match a realistic Cardputer Zero sysfs layout. The board's BQ27220 must
    // win over an unrelated battery-class device regardless of directory order.
    write_battery(root, "aaa-usb-pack", 1, 88, "Discharging");
    write_battery(root, "bq27220-0", 1, 42, "Charging");
    std::filesystem::create_directories(root / "network" / "wlan0" / "wireless");
    write(root / "network" / "wlan0" / "carrier", "1\n");
    write(root / "network" / "eth0" / "type", "1\n");
    write(root / "network" / "eth0" / "carrier", "0\n");
    write(root / "wireless",
          "Inter-| sta-|   Quality        |   Discarded packets               | Missed | WE\n"
          " face | tus | link level noise |  nwid  crypt   frag  retry   misc | beacon | 22\n"
          "wlan0: 0000   35.  -50.  -256        0      0      0      0      0        0\n");

    status = platform::read_device_status();
    assert(status.battery_present);
    assert(status.battery_charging);
    assert(status.battery_percent == 42);
    assert(status.wifi_present);
    assert(status.wifi_connected);
    assert(status.wifi_strength_percent == 50);
    assert(status.ethernet_present);
    assert(!status.ethernet_connected);

    write(root / "network" / "eth0" / "carrier", "1\n");
    status = platform::read_device_status();
    assert(status.ethernet_connected);

    // A physically absent board battery must not be replaced by an unrelated
    // USB/UPS battery when the preferred BQ device is still enumerated.
    write(root / "power" / "bq27220-0" / "present", "0\n");
    status = platform::read_device_status();
    assert(!status.battery_present);

    // If no BQ device exists, a standards-compliant generic battery remains a
    // useful fallback for desktop and compatible Linux targets.
    std::filesystem::remove_all(root / "power" / "bq27220-0");
    status = platform::read_device_status();
    assert(status.battery_present);
    assert(!status.battery_charging);
    assert(status.battery_percent == 88);

    // Bad sysfs capacity must not leak into the UI as a plausible value.
    write(root / "power" / "aaa-usb-pack" / "capacity", "101\n");
    status = platform::read_device_status();
    assert(!status.battery_present);

    std::filesystem::remove_all(root, ec);
    return 0;
}
