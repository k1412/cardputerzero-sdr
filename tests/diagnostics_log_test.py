#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


def run(script: str, log: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, script, "--p0", str(log)],
        check=False,
        capture_output=True,
        text=True,
    )


def main() -> int:
    assert len(sys.argv) == 2
    script = sys.argv[1]
    good_record = (
        "[zero-sdr][I] diagnostics uptime_ms=1800000 frequency_hz=97400000 muted=0 "
        "ui_loops=300000 max_ui_gap_ms=12 connection_attempts=1 "
        "successful_connections=1 retry_waits=0 read_errors=0 settings_updates=2 "
        "iq_blocks=450000 iq_bytes=7372800000 audio_generated=57600000 "
        "audio_written=57600000 audio_dropped=0 audio_recoveries=0 "
        "audio_write_errors=0 audio_open_failures=0 total_processing_us=450000000 "
        "max_processing_us=3500\n"
    )
    with tempfile.TemporaryDirectory() as directory:
        log = Path(directory) / "receiver.log"
        log.write_text(good_record, encoding="utf-8")
        result = run(script, log)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "Overall: PASS" in result.stdout
        assert "4,096,000 bytes/s" in result.stdout

        log.write_text(good_record.replace("read_errors=0", "read_errors=2"),
                       encoding="utf-8")
        result = run(script, log)
        assert result.returncode == 1, result.stdout + result.stderr
        assert "FAIL: zero RTL-SDR read errors" in result.stdout
        assert "Overall: FAIL" in result.stdout
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
