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


def main() -> int:
    assert len(sys.argv) == 2
    tool = sys.argv[1]
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        sys_root = root / "sys"
        dev_root = root / "dev"
        proc_root = root / "proc"
        output = root / "evidence"

        write(sys_root / "class/graphics/fb0/name", "fake-fb\n")
        write(sys_root / "class/graphics/fb0/virtual_size", "320,170\n")
        write(sys_root / "class/graphics/fb0/bits_per_pixel", "16\n")
        write(dev_root / "fb0", "")
        write(sys_root / "class/input/event0/device/name", "Cardputer Keyboard\n")
        write(sys_root / "class/input/event0/device/capabilities/key", "1 2 3\n")
        write(dev_root / "input/event0", "")
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
        write(proc_root / "asound/cards", " 0 [Fake]: Fake Audio\n")
        write(proc_root / "asound/pcm", "00-00: Fake PCM : playback 1\n")
        write(proc_root / "meminfo", "MemTotal: 1024000 kB\nMemAvailable: 512000 kB\n")
        write(sys_root / "class/thermal/thermal_zone0/type", "cpu\n")
        write(sys_root / "class/thermal/thermal_zone0/temp", "42000\n")

        environment = dict(os.environ)
        environment.update({
            "ZERO_SDR_P0_SYS_ROOT": str(sys_root),
            "ZERO_SDR_P0_DEV_ROOT": str(dev_root),
            "ZERO_SDR_P0_PROC_ROOT": str(proc_root),
            "ZERO_SDR_P0_APP": "/bin/true",
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
        assert "preflight=PASS" in report
        assert "framebuffer_virtual_size=320,170" in report
        assert "input_events=1 readable=1" in report
        assert "rtl_devices=1 accessible=1" in report
        assert "rtl_usb=0bda:2832" in report
        assert "SECRET-SERIAL-MUST-NOT-LEAK" not in report
        assert "hostname=" not in report
        assert output.stat().st_mode & 0o777 == 0o700
        assert (output / "preflight.txt").stat().st_mode & 0o777 == 0o600

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
        assert "app_exit_status=0" in short_result
        assert "forced_kill=0" in short_result

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
