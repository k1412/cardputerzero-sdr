#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

"""Summarize Zero SDR structured health logs and optionally enforce P0 gates."""

from __future__ import annotations

import argparse
import csv
import re
import sys
from pathlib import Path


EXPECTED_IQ_BYTES_PER_SECOND = 4_096_000.0
EXPECTED_AUDIO_FRAMES_PER_SECOND = 32_000.0
IQ_BLOCK_BUDGET_US = 4_000.0
P0_RUNTIME_SECONDS = 30 * 60
P0_DIAGNOSTICS_INTERVAL_SECONDS = 30
FIELD_PATTERN = re.compile(r"\b([a-z_]+)=([0-9]+)\b")
RESOURCE_FIELDS = [
    "epoch", "pid", "state", "elapsed_s", "cpu_percent", "rss_kib",
    "vsz_kib", "mem_available_kib", "max_temp_millic",
    "battery_present", "battery_status", "battery_capacity_percent",
    "battery_voltage_uv", "battery_current_ua", "battery_temp_tenths_c",
]
MONOTONIC_FIELDS = {
    "uptime_ms", "ui_loops", "max_ui_gap_ms", "connection_attempts",
    "successful_connections", "retry_waits", "read_errors",
    "settings_updates", "iq_blocks", "iq_bytes", "audio_generated",
    "audio_written", "audio_dropped", "audio_recoveries",
    "audio_write_errors", "audio_open_failures", "total_processing_us",
    "max_processing_us",
}


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


def load_key_values(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value
    return values


def integer_value(values: dict[str, str], key: str) -> int | None:
    try:
        return int(values[key])
    except (KeyError, ValueError):
        return None


def integer_in_range(value: int | None, minimum: int, maximum: int) -> bool:
    return value is not None and minimum <= value <= maximum


def load_resource_rows(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(encoding="utf-8", errors="replace", newline="") as stream:
        reader = csv.DictReader(stream)
        return list(reader.fieldnames or []), list(reader)


def snapshots_are_monotonic(snapshots: list[dict[str, int]]) -> bool:
    if not snapshots or any(not MONOTONIC_FIELDS <= snapshot.keys()
                            for snapshot in snapshots):
        return False
    return all(
        current[field] >= previous[field]
        for previous, current in zip(snapshots, snapshots[1:])
        for field in MONOTONIC_FIELDS
    )


def p0_evidence_checks(evidence_dir: Path,
                       snapshots: list[dict[str, int]],
                       runtime_seconds: float) -> list[tuple[bool, str]]:
    preflight_path = evidence_dir / "preflight.txt"
    result_path = evidence_dir / "result.txt"
    resources_path = evidence_dir / "resources.csv"
    checks: list[tuple[bool, str]] = []

    try:
        preflight_text = preflight_path.read_text(
            encoding="utf-8", errors="replace")
        preflight = load_key_values(preflight_path)
    except OSError:
        preflight_text = ""
        preflight = {}
    battery_capacity = integer_value(preflight, "battery_capacity_percent")
    battery_voltage = integer_value(preflight, "battery_voltage_uv")
    battery_current = integer_value(preflight, "battery_current_ua")
    battery_temp = integer_value(preflight, "battery_temp_tenths_c")
    battery_preflight_ok = (
        "bq27" in preflight.get("battery_supply", "") and
        preflight.get("battery_present") == "1" and
        integer_in_range(battery_capacity, 0, 100) and
        integer_in_range(battery_voltage, 0, 20_000_000) and
        integer_in_range(battery_current, -5_000_499, 5_000_499) and
        integer_in_range(battery_temp, -400, 1_000) and
        preflight.get("battery_status") in
        {"Charging", "Discharging", "Not charging", "Full"}
    )
    checks.extend([
        (preflight.get("schema") == "zero-sdr-p0-v5",
         "recognized P0 preflight schema"),
        (preflight.get("preflight") == "PASS",
         "device preflight passed"),
        (preflight.get("device_model", "").split(" path=", 1)[0] not in
         {"", "missing"},
         "device-tree model was recorded"),
        (preflight.get("cardputerzero_overlay", "").split(" path=", 1)[0] in
         {"cardputerzero-overlay", "cardputerzero-v3-overlay",
          "cardputerzero-v5-overlay"},
         "official Cardputer Zero device-tree overlay was active"),
        (preflight.get("launcher_service") in {"active", "inactive"},
         "APPLaunch user-service state was observed"),
        (preflight.get("app_processes") == "0",
         "no concurrent Zero SDR process at preflight"),
        (preflight.get("plugdev_membership") == "present",
         "normal user belongs to plugdev"),
        (preflight.get("monitor_tools") == "ok",
         "all evidence tools were available"),
        (preflight.get("package", "").startswith("install ok installed ") and
         preflight.get("package", "").endswith(" arm64"),
         "ARM64 Zero SDR package was installed"),
        (preflight.get("rtl_runtime_package", "").startswith(
            "install ok installed "),
         "RTL-SDR runtime package was installed"),
        (preflight.get("rtl_udev_rule", "").startswith("valid path="),
         "distribution RTL-SDR udev rule was valid"),
        (battery_preflight_ok,
         "Cardputer BQ battery telemetry passed preflight"),
        ("hostname=" not in preflight_text and
         "serial=" not in preflight_text and
         "network=" not in preflight_text,
         "preflight excludes hostname, USB serial, and network data"),
    ])

    try:
        result = load_key_values(result_path)
    except OSError:
        result = {}
    requested_seconds = integer_value(result, "requested_seconds")
    result_runtime = integer_value(result, "runtime_seconds")
    sample_interval = integer_value(result, "sample_interval_seconds")
    diagnostics_records = integer_value(result, "diagnostics_records")
    launcher_was_active = integer_value(result, "launcher_was_active")
    launcher_transition_ok = (
        launcher_was_active == 1 and
        result.get("launcher_stop_status") == "0" and
        result.get("launcher_restart_status") == "0"
    ) or (
        launcher_was_active == 0 and
        result.get("launcher_stop_status") == "not-needed" and
        result.get("launcher_restart_status") == "not-needed"
    )
    expected_diagnostics = (
        requested_seconds // P0_DIAGNOSTICS_INTERVAL_SECONDS
        if requested_seconds is not None else 0
    )
    diagnostic_coverage = (
        expected_diagnostics > 0 and
        len(snapshots) >= max(2, int(expected_diagnostics * 0.8)) and
        snapshots[0].get("uptime_ms", P0_RUNTIME_SECONDS * 1000) <=
        P0_DIAGNOSTICS_INTERVAL_SECONDS * 2 * 1000 and
        snapshots[-1].get("uptime_ms", 0) >= requested_seconds * 1000
    )
    checks.extend([
        (result.get("schema") == "zero-sdr-p0-result-v4",
         "recognized P0 result schema"),
        (requested_seconds is not None and
         requested_seconds >= P0_RUNTIME_SECONDS,
         "evidence run requested at least 30 minutes"),
        (result_runtime is not None and requested_seconds is not None and
         result_runtime >= requested_seconds,
         "runner stayed active for the requested duration"),
        (result.get("duration_complete") == "1",
         "runner marked the duration complete"),
        (result.get("app_exit_status") == "0" and
         result.get("forced_kill") == "0",
         "app exited cleanly without a forced kill"),
        (launcher_transition_ok,
         "APPLaunch pause/restore transition completed correctly"),
        (sample_interval is not None and 1 <= sample_interval <= 300,
         "resource sampling interval was recorded"),
        (diagnostics_records == len(snapshots),
         "result diagnostics count matches app.log"),
        (diagnostic_coverage,
         "diagnostic records cover the requested run at the expected cadence"),
        (snapshots_are_monotonic(snapshots),
         "diagnostic uptime and cumulative counters are monotonic"),
        (result_runtime is not None and sample_interval is not None and
         abs(result_runtime - runtime_seconds) <= sample_interval + 15,
         "runner and application runtimes agree"),
    ])

    try:
        resource_fields, resource_rows = load_resource_rows(resources_path)
    except (OSError, csv.Error):
        resource_fields, resource_rows = [], []
    resource_shape_ok = resource_fields == RESOURCE_FIELDS and bool(resource_rows)
    elapsed_values: list[int] = []
    resource_values_ok = resource_shape_ok
    pids: set[int] = set()
    battery_presents: list[int] = []
    battery_statuses: list[str] = []
    battery_capacities: list[int] = []
    battery_voltages: list[int] = []
    battery_currents: list[int] = []
    battery_temperatures: list[int] = []
    if resource_shape_ok:
        try:
            for row in resource_rows:
                pids.add(int(row["pid"]))
                elapsed_values.append(int(row["elapsed_s"]))
                float(row["cpu_percent"])
                int(row["rss_kib"])
                int(row["vsz_kib"])
                if row["mem_available_kib"] != "unknown":
                    int(row["mem_available_kib"])
                if row["max_temp_millic"] != "unknown":
                    int(row["max_temp_millic"])
                battery_presents.append(int(row["battery_present"]))
                battery_statuses.append(row["battery_status"])
                battery_capacities.append(int(row["battery_capacity_percent"]))
                battery_voltages.append(int(row["battery_voltage_uv"]))
                battery_currents.append(int(row["battery_current_ua"]))
                battery_temperatures.append(int(row["battery_temp_tenths_c"]))
                if row["state"].startswith("Z"):
                    resource_values_ok = False
        except (KeyError, TypeError, ValueError):
            resource_values_ok = False
    battery_samples_ok = (
        len(battery_presents) == len(battery_statuses) ==
        len(battery_capacities) == len(resource_rows) > 0 and
        all(value == 1 for value in battery_presents) and
        all(value in {"Charging", "Discharging", "Not charging", "Full"}
            for value in battery_statuses) and
        all(0 <= value <= 100 for value in battery_capacities) and
        all(0 <= value <= 20_000_000 for value in battery_voltages) and
        all(-5_000_499 <= value <= 5_000_499 for value in battery_currents) and
        all(-400 <= value <= 1_000 for value in battery_temperatures)
    )
    if battery_samples_ok:
        statuses = "/".join(sorted(set(battery_statuses)))
        battery_description = (
            "continuous board-battery telemetry is valid "
            f"(present throughout, status {statuses}, "
            f"capacity {min(battery_capacities)}-{max(battery_capacities)}%, "
            f"voltage {min(battery_voltages) / 1000:.0f}-"
            f"{max(battery_voltages) / 1000:.0f} mV, "
            f"current {min(battery_currents) / 1000:.0f}-"
            f"{max(battery_currents) / 1000:.0f} mA)"
        )
    else:
        battery_description = "continuous board-battery telemetry is valid"
    resource_monotonic = (
        bool(elapsed_values) and
        elapsed_values == sorted(elapsed_values) and
        len(pids) == 1
    )
    resource_coverage = (
        bool(elapsed_values) and requested_seconds is not None and
        elapsed_values[0] <= 5 and elapsed_values[-1] >= requested_seconds
    )
    expected_samples = (
        requested_seconds // sample_interval
        if requested_seconds is not None and sample_interval else 0
    )
    sample_count_ok = (
        expected_samples > 0 and
        len(resource_rows) >= max(2, int(expected_samples * 0.8))
    )
    checks.extend([
        (resource_shape_ok, "resource CSV has the expected schema and samples"),
        (resource_values_ok and resource_monotonic,
         "resource samples are valid, ordered, and belong to one process"),
        (battery_samples_ok, battery_description),
        (resource_coverage,
         "resource samples cover the requested run"),
        (sample_count_ok,
         "resource sample count is consistent with the interval"),
    ])
    return checks


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
    parser.add_argument(
        "input",
        help="application log path, or a complete evidence directory with --p0")
    parser.add_argument(
        "--p0", action="store_true",
        help="enforce the 30-minute continuous-capture release gates")
    args = parser.parse_args()

    if args.p0:
        evidence_dir = Path(args.input)
        if not evidence_dir.is_dir():
            print("ERROR: --p0 requires the complete evidence directory, not only app.log",
                  file=sys.stderr)
            return 2
        log_path = evidence_dir / "app.log"
        log_input = str(log_path)
    else:
        evidence_dir = None
        log_input = args.input

    try:
        snapshots = load_snapshots(log_input)
    except OSError as error:
        print(f"ERROR: unable to read {log_input}: {error}", file=sys.stderr)
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

    assert evidence_dir is not None
    print("P0 evidence-integrity checks")
    evidence_checks = p0_evidence_checks(
        evidence_dir, snapshots, runtime_seconds)
    for passed, description in evidence_checks:
        print(f"  {'PASS' if passed else 'FAIL'}: {description}")

    print("P0 continuous-capture checks")
    capture_checks = p0_checks(metrics, runtime_seconds, iq_rate,
                               audio_written_rate, average_processing_us)
    for passed, description in capture_checks:
        print(f"  {'PASS' if passed else 'FAIL'}: {description}")
    passed = all(result for result, _ in evidence_checks + capture_checks)
    print(f"Overall: {'PASS' if passed else 'FAIL'}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
