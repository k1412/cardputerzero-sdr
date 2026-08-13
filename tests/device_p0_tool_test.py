#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


def write(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value, encoding="utf-8")


def write_executable(path: Path, value: str) -> None:
    write(path, value)
    path.chmod(0o755)


def main() -> int:
    assert len(sys.argv) == 2
    tool = sys.argv[1]
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        sys_root = root / "sys"
        dev_root = root / "dev"
        proc_root = root / "proc"
        output = root / "evidence"
        fake_bin = root / "bin"
        launcher_state = root / "launcher-state"
        launcher_log = root / "launcher-log"

        write_executable(
            fake_bin / "id",
            """#!/bin/sh
case "${1:-}" in
    -u) echo 1000 ;;
    -g) echo 1000 ;;
    -Gn) echo "${FAKE_ID_GROUPS:-wuyang audio input plugdev video}" ;;
    *) exit 2 ;;
esac
""",
        )
        write_executable(
            fake_bin / "dpkg-query",
            """#!/bin/sh
package=""
for package do :; done
case "$package" in
    cardputerzero-sdr)
        printf 'install ok installed 0.1.0-8 arm64'
        ;;
    librtlsdr0)
        [ "${FAKE_RTL_PACKAGE_MISSING:-0}" = 0 ] || exit 1
        printf 'install ok installed 2.0.2-2+b1 arm64'
        ;;
    *) exit 1 ;;
esac
""",
        )
        write_executable(
            fake_bin / "systemctl",
            """#!/bin/sh
[ "${1:-}" = --user ] || exit 2
shift
case "${1:-}" in
    is-active)
        shift
        quiet=0
        if [ "${1:-}" = --quiet ]; then
            quiet=1
            shift
        fi
        [ "${1:-}" = APPLaunch.service ] || exit 2
        state=$(cat "$FAKE_SYSTEMCTL_STATE")
        case "$state" in
            active)
                [ "$quiet" -eq 1 ] || echo active
                exit 0
                ;;
            inactive)
                [ "$quiet" -eq 1 ] || echo inactive
                exit 3
                ;;
            *) exit 1 ;;
        esac
        ;;
    stop)
        [ "${2:-}" = APPLaunch.service ] || exit 2
        [ "${FAKE_SYSTEMCTL_STOP_FAIL:-0}" = 0 ] || exit 1
        echo stop >>"$FAKE_SYSTEMCTL_LOG"
        printf 'inactive\n' >"$FAKE_SYSTEMCTL_STATE"
        ;;
    start)
        [ "${2:-}" = APPLaunch.service ] || exit 2
        echo start >>"$FAKE_SYSTEMCTL_LOG"
        printf 'active\n' >"$FAKE_SYSTEMCTL_STATE"
        ;;
    *) exit 2 ;;
esac
""",
        )

        write(launcher_state, "active\n")

        write(root / "etc/os-release", 'ID=debian\nVERSION_ID="13"\n')
        write(sys_root / "firmware/devicetree/base/model",
              "M5Stack Cardputer Zero\0")
        boot_config = root / "boot/firmware/config.txt"
        write(boot_config, "dtoverlay=cardputerzero-v5-overlay\n")
        write(sys_root / "class/graphics/fb0/name", "fake-fb\n")
        write(sys_root / "class/graphics/fb0/virtual_size", "320,170\n")
        write(sys_root / "class/graphics/fb0/bits_per_pixel", "16\n")
        write(dev_root / "fb0", "")
        write(sys_root / "class/input/event0/device/name", "Cardputer Keyboard\n")
        write(sys_root / "class/input/event0/device/capabilities/key", "1 2 3\n")
        write(dev_root / "input/event0", "")
        preferred_keyboard = dev_root / "input/by-path/platform-3f804000.i2c-event"
        preferred_keyboard.parent.mkdir(parents=True, exist_ok=True)
        preferred_keyboard.symlink_to(Path("../event0"))
        usb = sys_root / "bus/usb/devices/1-1"
        write(usb / "idVendor", "0bda\n")
        write(usb / "idProduct", "2832\n")
        write(usb / "busnum", "1\n")
        write(usb / "devnum", "2\n")
        write(usb / "speed", "480\n")
        write(usb / "bMaxPower", "500mA\n")
        write(usb / "power/runtime_status", "active\n")
        write(usb / "serial", "SECRET-SERIAL-MUST-NOT-LEAK\n")
        write(dev_root / "bus/usb/001/002", "")
        udev_rule = root / "usr/lib/udev/rules.d/60-librtlsdr0.rules"
        udev_rule_text = (
            'SUBSYSTEMS=="usb", ATTRS{idVendor}=="0bda", '
            'ATTRS{idProduct}=="2832", MODE="0660", GROUP="plugdev"\n'
            'SUBSYSTEMS=="usb", ATTRS{idVendor}=="0bda", '
            'ATTRS{idProduct}=="2838", MODE="0660", GROUP="plugdev"\n'
        )
        write(udev_rule, udev_rule_text)
        write(proc_root / "asound/cards", " 0 [Fake]: Fake Audio\n")
        write(proc_root / "asound/pcm", "00-00: Fake PCM : playback 1\n")
        write(dev_root / "snd/pcmC0D0p", "")
        write(proc_root / "meminfo", "MemTotal: 1024000 kB\nMemAvailable: 512000 kB\n")
        write(sys_root / "class/thermal/thermal_zone0/type", "cpu\n")
        write(sys_root / "class/thermal/thermal_zone0/temp", "42000\n")
        generic_battery = sys_root / "class/power_supply/aaa-usb-pack"
        write(generic_battery / "type", "Battery\n")
        write(generic_battery / "present", "1\n")
        write(generic_battery / "capacity", "44\n")
        write(generic_battery / "status", "Discharging\n")
        write(generic_battery / "voltage_now", "3700000\n")
        write(generic_battery / "current_now", "-90000\n")
        write(generic_battery / "temp", "250\n")
        battery = sys_root / "class/power_supply/bq27220-0"
        write(battery / "type", "Battery\n")
        write(battery / "present", "1\n")
        write(battery / "capacity", "81\n")
        write(battery / "status", "Charging\n")
        write(battery / "voltage_now", "4100123\n")
        write(battery / "current_instant", "125500\n")
        write(battery / "temp", "273\n")

        environment = dict(os.environ)
        environment.pop("LV_LINUX_FBDEV_DEVICE", None)
        environment.pop("APPLAUNCH_LINUX_FBDEV_DEVICE", None)
        environment.pop("LV_LINUX_KEYBOARD_DEVICE", None)
        environment.pop("APPLAUNCH_LINUX_KEYBOARD_DEVICE", None)
        environment.update({
            "PATH": str(fake_bin) + os.pathsep + environment["PATH"],
            "ZERO_SDR_P0_ROOT": str(root),
            "ZERO_SDR_P0_SYS_ROOT": str(sys_root),
            "ZERO_SDR_P0_DEV_ROOT": str(dev_root),
            "ZERO_SDR_P0_PROC_ROOT": str(proc_root),
            "ZERO_SDR_P0_APP": "/bin/true",
            "FAKE_SYSTEMCTL_STATE": str(launcher_state),
            "FAKE_SYSTEMCTL_LOG": str(launcher_log),
        })
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        report = (output / "preflight.txt").read_text(encoding="utf-8")
        assert "schema=zero-sdr-p0-v5" in report
        assert "preflight=PASS" in report
        assert "os_id=debian" in report
        assert "os_version=13" in report
        assert "device_model=M5Stack Cardputer Zero" in report
        assert "cardputerzero_overlay=cardputerzero-v5-overlay" in report
        assert "plugdev_membership=present" in report
        assert "launcher_service=active" in report
        assert "app_processes=0" in report
        assert "package=install ok installed 0.1.0-8 arm64" in report
        assert "rtl_runtime_package=install ok installed 2.0.2-2+b1 arm64" in report
        assert f"rtl_udev_rule=valid path={udev_rule}" in report
        assert "framebuffer_virtual_size=320,170" in report
        assert "input_events=1 readable=1" in report
        assert f"preferred_keyboard={preferred_keyboard} access=read-write" in report
        assert "rtl_devices=1 accessible=1 high_speed=1" in report
        assert "rtl_usb=0bda:2832" in report
        assert "alsa_playback_nodes=1 accessible=1" in report
        assert "battery_supply=bq27220-0" in report
        assert "battery_present=1" in report
        assert "battery_capacity_percent=81" in report
        assert "battery_status=Charging" in report
        assert "battery_voltage_uv=4100123" in report
        assert "battery_current_ua=125500" in report
        assert "battery_temp_tenths_c=273" in report
        assert "SECRET-SERIAL-MUST-NOT-LEAK" not in report
        assert "hostname=" not in report
        assert output.stat().st_mode & 0o777 == 0o700
        assert (output / "preflight.txt").stat().st_mode & 0o777 == 0o600
        assert launcher_state.read_text(encoding="utf-8") == "active\n"
        assert not launcher_log.exists()

        write(sys_root / "class/graphics/fb1/name", "redirected-fb\n")
        write(sys_root / "class/graphics/fb1/virtual_size", "320,170\n")
        write(sys_root / "class/graphics/fb1/bits_per_pixel", "16\n")
        write(dev_root / "fb1", "")
        redirected_environment = dict(environment)
        redirected_environment["APPLAUNCH_LINUX_FBDEV_DEVICE"] = str(dev_root / "fb1")
        redirected_output = root / "redirected-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(redirected_output)],
            env=redirected_environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        redirected_report = (
            redirected_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert f"framebuffer={dev_root / 'fb1'} access=read-write" in redirected_report
        assert "framebuffer_name=redirected-fb" in redirected_report
        assert "preflight=PASS" in redirected_report

        lv_precedence_environment = dict(redirected_environment)
        lv_precedence_environment["LV_LINUX_FBDEV_DEVICE"] = str(dev_root / "fb0")
        lv_precedence_output = root / "lv-precedence-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(lv_precedence_output)],
            env=lv_precedence_environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0, result.stdout + result.stderr
        lv_precedence_report = (
            lv_precedence_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert f"framebuffer={dev_root / 'fb0'} access=read-write" in lv_precedence_report
        assert "framebuffer_name=fake-fb" in lv_precedence_report
        assert "framebuffer_name=redirected-fb" not in lv_precedence_report

        concurrent_process = proc_root / "4321"
        concurrent_process.mkdir()
        (concurrent_process / "exe").symlink_to("/bin/true")
        concurrent_output = root / "concurrent-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(concurrent_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        concurrent_report = (
            concurrent_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert "app_processes=1" in concurrent_report
        assert "preflight=FAIL" in concurrent_report
        (concurrent_process / "exe").unlink()
        concurrent_process.rmdir()

        write(launcher_state, "unavailable\n")
        unavailable_launcher_output = root / "unavailable-launcher-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(unavailable_launcher_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        unavailable_launcher_report = (
            unavailable_launcher_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert "launcher_service=unavailable" in unavailable_launcher_report
        assert "preflight=FAIL" in unavailable_launcher_report
        assert not launcher_log.exists()
        write(launcher_state, "active\n")

        boot_config.unlink()
        missing_overlay_output = root / "missing-overlay-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(missing_overlay_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        missing_overlay_report = (
            missing_overlay_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert "cardputerzero_overlay=missing" in missing_overlay_report
        assert "preflight=FAIL" in missing_overlay_report
        write(boot_config, "dtoverlay=cardputerzero-v5-overlay\n")

        write(battery / "present", "0\n")
        missing_battery_output = root / "missing-battery-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(missing_battery_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        missing_battery_report = (
            missing_battery_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert "battery_supply=bq27220-0" in missing_battery_report
        assert "battery_present=0" in missing_battery_report
        assert "preflight=FAIL" in missing_battery_report
        write(battery / "present", "1\n")

        missing_dependency_environment = dict(environment)
        missing_dependency_environment["FAKE_RTL_PACKAGE_MISSING"] = "1"
        missing_dependency_output = root / "missing-dependency-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(missing_dependency_output)],
            env=missing_dependency_environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        missing_dependency_report = (
            missing_dependency_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert "rtl_runtime_package=not-installed" in missing_dependency_report
        assert "preflight=FAIL" in missing_dependency_report

        missing_group_environment = dict(environment)
        missing_group_environment["FAKE_ID_GROUPS"] = "wuyang audio input video"
        missing_group_output = root / "missing-group-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(missing_group_output)],
            env=missing_group_environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        missing_group_report = (
            missing_group_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert "plugdev_membership=missing" in missing_group_report
        assert "preflight=FAIL" in missing_group_report

        udev_rule.unlink()
        missing_rule_output = root / "missing-rule-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(missing_rule_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        missing_rule_report = (
            missing_rule_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert "rtl_udev_rule=missing" in missing_rule_report
        assert "preflight=FAIL" in missing_rule_report
        write(udev_rule, udev_rule_text)

        write(usb / "speed", "12\n")
        low_speed_output = root / "low-speed-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(low_speed_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        low_speed_report = (
            low_speed_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert "rtl_devices=1 accessible=1 high_speed=0" in low_speed_report
        assert "preflight=FAIL" in low_speed_report
        write(usb / "speed", "480\n")

        short_output = root / "short-evidence"
        result = subprocess.run(
            [tool, "--duration", "60", "--interval", "1",
             "--output", str(short_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        short_result = (short_output / "result.txt").read_text(encoding="utf-8")
        assert "duration_complete=0" in short_result
        assert "schema=zero-sdr-p0-result-v3" in short_result
        assert "sample_interval_seconds=1" in short_result
        assert "app_exit_status=0" in short_result
        assert "forced_kill=0" in short_result
        assert "launcher_was_active=1" in short_result
        assert "launcher_stop_status=0" in short_result
        assert "launcher_restart_status=0" in short_result
        assert launcher_state.read_text(encoding="utf-8") == "active\n"
        assert launcher_log.read_text(encoding="utf-8").splitlines() == [
            "stop",
            "start",
        ]

        launcher_log.unlink()
        failed_pause_environment = dict(environment)
        failed_pause_environment["FAKE_SYSTEMCTL_STOP_FAIL"] = "1"
        failed_pause_output = root / "failed-pause-evidence"
        result = subprocess.run(
            [tool, "--duration", "60", "--interval", "1",
             "--output", str(failed_pause_output)],
            env=failed_pause_environment,
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        assert "failed to pause APPLaunch.service" in result.stderr
        assert launcher_state.read_text(encoding="utf-8") == "active\n"
        assert launcher_log.read_text(encoding="utf-8").splitlines() == ["start"]
        assert not (failed_pause_output / "app.log").exists()

        launcher_log.unlink()
        write(launcher_state, "inactive\n")
        inactive_output = root / "inactive-launcher-evidence"
        result = subprocess.run(
            [tool, "--duration", "60", "--interval", "1",
             "--output", str(inactive_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        inactive_result = (inactive_output / "result.txt").read_text(
            encoding="utf-8"
        )
        assert "launcher_was_active=0" in inactive_result
        assert "launcher_stop_status=not-needed" in inactive_result
        assert "launcher_restart_status=not-needed" in inactive_result
        assert launcher_state.read_text(encoding="utf-8") == "inactive\n"
        assert not launcher_log.exists()
        write(launcher_state, "active\n")

        (dev_root / "snd/pcmC0D0p").unlink()
        no_audio_output = root / "no-audio-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(no_audio_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        no_audio_report = (no_audio_output / "preflight.txt").read_text(encoding="utf-8")
        assert "alsa_playback_nodes=0 accessible=0" in no_audio_report
        assert "preflight=FAIL" in no_audio_report
        write(dev_root / "snd/pcmC0D0p", "")

        write(sys_root / "class/graphics/fb0/virtual_size", "240,135\n")
        wrong_display_output = root / "wrong-display-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(wrong_display_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        wrong_display_report = (
            wrong_display_output / "preflight.txt"
        ).read_text(encoding="utf-8")
        assert "framebuffer_virtual_size=240,135" in wrong_display_report
        assert "preflight=FAIL" in wrong_display_report
        write(sys_root / "class/graphics/fb0/virtual_size", "320,170\n")

        (dev_root / "bus/usb/001/002").unlink()
        failed_output = root / "failed-evidence"
        result = subprocess.run(
            [tool, "--preflight-only", "--output", str(failed_output)],
            env=environment,
            check=False,
            capture_output=True,
            text=True,
        )
        assert result.returncode == 1, result.stdout + result.stderr
        failed_report = (failed_output / "preflight.txt").read_text(encoding="utf-8")
        assert "rtl_devices=1 accessible=0" in failed_report
        assert "preflight=FAIL" in failed_report
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
