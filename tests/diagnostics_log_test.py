#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


def run(
    script: str, path: Path, *, p0: bool = True
) -> subprocess.CompletedProcess[str]:
    command = [sys.executable, script]
    if p0:
        command.append("--p0")
    command.append(str(path))
    return subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
    )


def diagnostics_record(seconds: int) -> str:
    iq_blocks = seconds * 250
    iq_bytes = seconds * 4_096_000
    audio_frames = seconds * 32_000
    return (
        f"[zero-sdr][I] diagnostics uptime_ms={seconds * 1000} "
        "frequency_hz=97400000 muted=0 "
        f"ui_loops={seconds * 160} max_ui_gap_ms=12 "
        "connection_attempts=1 successful_connections=1 retry_waits=0 "
        "read_errors=0 settings_updates=2 "
        f"iq_blocks={iq_blocks} iq_bytes={iq_bytes} "
        f"audio_generated={audio_frames} audio_written={audio_frames} "
        "audio_dropped=0 audio_recoveries=0 audio_write_errors=0 "
        "audio_open_failures=0 "
        f"total_processing_us={iq_blocks * 1000} max_processing_us=3500\n"
    )


def write_good_evidence(evidence: Path) -> None:
    evidence.mkdir()
    records = "".join(diagnostics_record(seconds)
                      for seconds in range(30, 1801, 30))
    (evidence / "app.log").write_text(records, encoding="utf-8")
    (evidence / "preflight.txt").write_text(
        """schema=zero-sdr-p0-v4
uid=1000
plugdev_membership=present
monitor_tools=ok
launcher_service=active
device_model=M5Stack Cardputer Zero path=/sys/firmware/devicetree/base/model
cardputerzero_overlay=cardputerzero-v5-overlay path=/boot/firmware/config.txt
package=install ok installed 0.1.0-6 arm64
rtl_runtime_package=install ok installed 2.0.2-2+b1 arm64
rtl_udev_rule=valid path=/usr/lib/udev/rules.d/60-librtlsdr0.rules
app_path=/usr/bin/cardputerzero-sdr
app_processes=0
preflight=PASS
""",
        encoding="utf-8",
    )
    (evidence / "result.txt").write_text(
        """schema=zero-sdr-p0-result-v2
started_epoch=1000
ended_epoch=2800
runtime_seconds=1800
requested_seconds=1800
sample_interval_seconds=30
duration_complete=1
app_exit_status=0
forced_kill=0
launcher_was_active=1
launcher_stop_status=0
launcher_restart_status=0
diagnostics_records=60
""",
        encoding="utf-8",
    )
    rows = [
        "epoch,pid,state,elapsed_s,cpu_percent,rss_kib,vsz_kib,"
        "mem_available_kib,max_temp_millic"
    ]
    rows.extend(
        f"{1000 + elapsed},4321,S,{elapsed},42.5,32768,65536,512000,48000"
        for elapsed in range(0, 1801, 30)
    )
    (evidence / "resources.csv").write_text(
        "\n".join(rows) + "\n", encoding="utf-8")


def main() -> int:
    assert len(sys.argv) == 2
    script = sys.argv[1]
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        evidence = root / "evidence"
        write_good_evidence(evidence)

        result = run(script, evidence)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "P0 evidence-integrity checks" in result.stdout
        assert "PASS: APPLaunch pause/restore transition completed correctly" in result.stdout
        assert "PASS: diagnostic records cover the requested run" in result.stdout
        assert "PASS: resource samples cover the requested run" in result.stdout
        assert "P0 continuous-capture checks" in result.stdout
        assert "Overall: PASS" in result.stdout
        assert "4,096,000 bytes/s" in result.stdout

        result = run(script, evidence / "app.log")
        assert result.returncode == 2, result.stdout + result.stderr
        assert "--p0 requires the complete evidence directory" in result.stderr

        result = run(script, evidence / "app.log", p0=False)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "Zero SDR diagnostics summary" in result.stdout
        assert "P0 evidence-integrity checks" not in result.stdout

        good_log = (evidence / "app.log").read_text(encoding="utf-8")
        (evidence / "app.log").write_text(
            good_log.replace("read_errors=0", "read_errors=2"),
            encoding="utf-8",
        )
        result = run(script, evidence)
        assert result.returncode == 1, result.stdout + result.stderr
        assert "FAIL: zero RTL-SDR read errors" in result.stdout
        assert "Overall: FAIL" in result.stdout
        (evidence / "app.log").write_text(good_log, encoding="utf-8")

        result_path = evidence / "result.txt"
        good_result = result_path.read_text(encoding="utf-8")
        result_path.write_text(
            good_result.replace("launcher_restart_status=0",
                                "launcher_restart_status=1"),
            encoding="utf-8",
        )
        result = run(script, evidence)
        assert result.returncode == 1, result.stdout + result.stderr
        assert "FAIL: APPLaunch pause/restore transition completed correctly" in result.stdout
        result_path.write_text(good_result, encoding="utf-8")

        resources_path = evidence / "resources.csv"
        good_resources = resources_path.read_text(encoding="utf-8")
        resources_path.write_text(
            "\n".join(good_resources.splitlines()[:-1]) + "\n",
            encoding="utf-8",
        )
        result = run(script, evidence)
        assert result.returncode == 1, result.stdout + result.stderr
        assert "FAIL: resource samples cover the requested run" in result.stdout
        resources_path.write_text(good_resources, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
