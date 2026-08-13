#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

"""Summarize Zero SDR structured health logs and optionally enforce P0 gates."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


EXPECTED_IQ_BYTES_PER_SECOND = 4_096_000.0
EXPECTED_AUDIO_FRAMES_PER_SECOND = 32_000.0
IQ_BLOCK_BUDGET_US = 4_000.0
P0_RUNTIME_SECONDS = 30 * 60
FIELD_PATTERN = re.compile(r"\b([a-z_]+)=([0-9]+)\b")


def load_snapshots(path: str) -> list[dict[str, int]]:
    if path == "-":
        lines = sys.stdin
    else:
        lines = Path(path).open(encoding="utf-8", errors="replace")
    try:
        return [
            {key: int(value) for key, value in FIELD_PATTERN.findall(line)}
            for line in lines
            if "diagnostics " in line
        ]
    finally:
        if path != "-":
            lines.close()


def rate(value: int, runtime_seconds: float) -> float:
    return value / runtime_seconds if runtime_seconds > 0 else 0.0


def within_ten_percent(observed: float, expected: float) -> bool:
    return expected * 0.90 <= observed <= expected * 1.10


def p0_checks(metrics: dict[str, int], runtime_seconds: float,
              iq_rate: float, audio_written_rate: float,
              average_processing_us: float) -> list[tuple[bool, str]]:
    checks = [
        (runtime_seconds >= P0_RUNTIME_SECONDS,
         f"continuous runtime >= {P0_RUNTIME_SECONDS} s"),
        (metrics["connection_attempts"] == 1 and
         metrics["successful_connections"] == 1 and
         metrics["retry_waits"] == 0,
         "one successful connection with no reconnect wait"),
        (metrics["read_errors"] == 0, "zero RTL-SDR read errors"),
        (within_ten_percent(iq_rate, EXPECTED_IQ_BYTES_PER_SECOND),
         "IQ throughput within 10% of 4,096,000 bytes/s"),
        (average_processing_us < IQ_BLOCK_BUDGET_US,
         "average processing time below the 4,000 us IQ-block budget"),
        (metrics["audio_dropped"] == 0,
         "zero dropped audio frames"),
        (metrics["audio_recoveries"] == 0 and
         metrics["audio_write_errors"] == 0 and
         metrics["audio_open_failures"] == 0,
         "zero audio recoveries/write/open failures"),
        (metrics["muted"] == 0, "receiver is unmuted for audio validation"),
        (within_ten_percent(audio_written_rate,
                            EXPECTED_AUDIO_FRAMES_PER_SECOND),
         "audio output within 10% of 32,000 frames/s"),
    ]
    return checks


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Summarize structured diagnostics emitted by Zero SDR.")
    parser.add_argument("log", help="application log path, or - for standard input")
    parser.add_argument(
        "--p0", action="store_true",
        help="enforce the 30-minute continuous-capture release gates")
    args = parser.parse_args()

    try:
        snapshots = load_snapshots(args.log)
    except OSError as error:
        print(f"ERROR: unable to read {args.log}: {error}", file=sys.stderr)
        return 2
    if not snapshots:
        print("ERROR: no Zero SDR diagnostics records found", file=sys.stderr)
        return 2

    metrics = snapshots[-1]
    required = {
        "uptime_ms", "muted", "ui_loops", "max_ui_gap_ms",
        "connection_attempts", "successful_connections", "retry_waits",
        "read_errors", "settings_updates", "iq_blocks", "iq_bytes",
        "audio_generated", "audio_written", "audio_dropped",
        "audio_recoveries", "audio_write_errors", "audio_open_failures",
        "total_processing_us", "max_processing_us",
    }
    missing = sorted(required - metrics.keys())
    if missing:
        print(f"ERROR: latest diagnostics record is missing: {', '.join(missing)}",
              file=sys.stderr)
        return 2

    runtime_seconds = metrics["uptime_ms"] / 1_000.0
    iq_rate = rate(metrics["iq_bytes"], runtime_seconds)
    audio_generated_rate = rate(metrics["audio_generated"], runtime_seconds)
    audio_written_rate = rate(metrics["audio_written"], runtime_seconds)
    average_processing_us = (
        metrics["total_processing_us"] / metrics["iq_blocks"]
        if metrics["iq_blocks"] else 0.0
    )
    processing_load = average_processing_us / IQ_BLOCK_BUDGET_US * 100.0

    print("Zero SDR diagnostics summary")
    print(f"  records: {len(snapshots)}")
    print(f"  runtime: {runtime_seconds:.1f} s")
    print(f"  connection attempts/successes/retry waits: "
          f"{metrics['connection_attempts']}/{metrics['successful_connections']}/"
          f"{metrics['retry_waits']}")
    print(f"  IQ: {metrics['iq_blocks']} blocks, {iq_rate:,.0f} bytes/s, "
          f"{metrics['read_errors']} read errors")
    print(f"  audio generated/written: {audio_generated_rate:,.0f}/"
          f"{audio_written_rate:,.0f} frames/s")
    print(f"  audio dropped/recoveries/write errors/open failures: "
          f"{metrics['audio_dropped']}/{metrics['audio_recoveries']}/"
          f"{metrics['audio_write_errors']}/{metrics['audio_open_failures']}")
    print(f"  DSP average/max: {average_processing_us:,.0f}/"
          f"{metrics['max_processing_us']:,} us per IQ block "
          f"({processing_load:.1f}% single-worker load)")
    print(f"  UI loops/max gap: {metrics['ui_loops']}/{metrics['max_ui_gap_ms']} ms")

    if not args.p0:
        return 0

    print("P0 continuous-capture checks")
    checks = p0_checks(metrics, runtime_seconds, iq_rate,
                       audio_written_rate, average_processing_us)
    for passed, description in checks:
        print(f"  {'PASS' if passed else 'FAIL'}: {description}")
    passed = all(result for result, _ in checks)
    print(f"Overall: {'PASS' if passed else 'FAIL'}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
