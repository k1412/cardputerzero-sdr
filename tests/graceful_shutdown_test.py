#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import time


def main() -> int:
    assert len(sys.argv) == 2
    environment = dict(os.environ)
    environment.update({
        "SDL_VIDEODRIVER": "dummy",
        "ZERO_SDR_DEMO": "1",
    })
    with tempfile.TemporaryDirectory() as config_root:
        environment["XDG_CONFIG_HOME"] = config_root
        process = subprocess.Popen(
            [sys.argv[1]],
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        output = ""
        deadline = time.monotonic() + 5
        assert process.stdout is not None
        while "LVGL app started" not in output and time.monotonic() < deadline:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            output += line
        assert "LVGL app started" in output, output
        process.terminate()
        tail, _ = process.communicate(timeout=5)
        output += tail
        assert process.returncode == 0, output
        assert "termination signal received; shutting down cleanly" in output, output
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
