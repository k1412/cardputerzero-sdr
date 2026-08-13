# Device test plan

No check in this document may be marked passed from a desktop simulator or cross-build alone.

## Known host-side dongle baseline

The target sample was observed on a macOS host as:

- Realtek RTL2832U, USB ID `0bda:2832`
- Fitipower FC0012 tuner
- Serial `77771111153705700`
- Measured usable tuning range approximately 22.0–948.6 MHz
- Stable 2.048 MS/s capture for about five seconds with zero reported sample loss
- Observed FM peaks near 90.5, 94.5, 97.4, 103.9, and 106.1 MHz

This establishes the dongle baseline only. It does not prove Cardputer Zero USB-host compatibility, power margin, RF quality, or audio behavior.

## P0 release gates

| Area | Procedure | Pass criterion | Status |
| --- | --- | --- | --- |
| Install | Install release `.deb`, launch from APPLaunch | correct icon/title; clean start/exit | Pending device |
| Display | Inspect radio/settings in dark and light themes; repeat once with the framebuffer path exported by APPLaunch | selected path and native 320×170 mode are logged; no clipping, tearing, overlays, or unreadable text | Pending device |
| Keys | Exercise F/X/Z/C, Enter, Esc, G/M/L/T, number row, period, Backspace, and hold-to-repeat | one action per press; repeat is controlled; Esc returns safely | Pending device |
| USB discovery | Cold boot, hot-plug, unplug/replug RTL2832U | installed `librtlsdr0`, valid distro rule, `plugdev` membership, at least 480 Mb/s link, and read-write node; state changes without crash; UI stays responsive | Pending device |
| USB power | Capture continuously with screen/audio active | no brownout, disconnect loop, or thermal warning | Pending device |
| Tuning | Test 22.0, 97.4, and 948.6 MHz by stepping and direct entry; try 21.999/948.601 | valid values tune exactly; invalid values are rejected; no overflow/wrap | Pending device |
| RF capture | Run 2.048 MS/s for 30 minutes and summarize the structured log | no read errors/reconnects; IQ throughput within 10% of 4,096,000 bytes/s | Pending device |
| WFM audio | Receive a known station, mute/unmute, unplug device | intelligible audio; no loud transient; mute is immediate | Pending device |
| Performance | Record process CPU/memory plus diagnostic DSP/UI metrics | average DSP work below 4,000 µs/block; physical UI/max-latency baseline documented | Pending device |
| Locales | Open radio/settings in all ten languages | glyphs render; no clipping or crash | Pending device |
| Persistence | Change frequency/step/gain/mute/theme/language, restart app | all survive and app can recover from malformed config | Pending device |

## Failure injection

- Start with no dongle.
- Remove the dongle during a USB read.
- Deny access to the USB device: the localized access state must appear with replug guidance, live audio must remain stopped, and the app must recover without restart after access is restored.
- Claim the dongle from another SDR process: the localized busy state must appear with close-other-SDR guidance, then recover after that process exits.
- Deny access to the input event node.
- Supply a malformed config file.
- Fill or make the user config directory read-only.
- Force the SDR worker behind real time and verify bounded frame dropping.
- Stop audio output and verify the receiver/UI can shut down cleanly.

## 30-minute continuous-capture evidence

Run this separately from hot-plug and failure-injection tests so intentional reconnects do not contaminate the continuous-run counters. Do not run another Zero SDR instance from APPLaunch at the same time. As the normal non-root APPLaunch user, collect the access snapshot first:

```sh
cardputerzero-sdr-p0 --preflight-only
```

The preflight must report `schema=zero-sdr-p0-v4` and `preflight=PASS`. Confirm a non-empty `device_model`, one of the official `cardputerzero-overlay`, `cardputerzero-v3-overlay`, or `cardputerzero-v5-overlay` values, `app_processes=0`, `launcher_service=active` or `inactive`, `plugdev_membership=present`, both package records are installed ARM64 builds, `rtl_udev_rule=valid`, and at least one accessible RTL-SDR reports `high_speed=1`; then inspect its framebuffer, input-event, USB-node, and ALSA facts rather than bypassing a failure with root. Keep audio unmuted on a known WFM station, then start the evidence run and exercise the physical controls while it remains active:

```sh
cardputerzero-sdr-p0 --duration 1800
```

The full runner pauses an active `APPLaunch.service` as the same non-root user so the child exclusively owns the framebuffer, and restores it through the exit trap. Require `launcher_stop_status=0` and `launcher_restart_status=0` when `launcher_was_active=1`; `not-needed` is correct when the service was already inactive. The runner writes a mode-0700 evidence directory containing `preflight.txt`, `app.log`, `resources.csv`, and `result.txt`; the default location is below `$XDG_STATE_HOME/cardputerzero-sdr/evidence/` or `~/.local/state/`. It excludes hostname, network state, and USB serials. Copy that directory into a repository checkout on the development machine, then run:

```sh
python3 scripts/summarize_diagnostics.py --p0 path/to/evidence
```

The evaluator requires the complete directory. It cross-checks `preflight.txt`,
`result.txt`, `resources.csv`, and `app.log`; a log copied without its launcher,
process-exit, package/access, and resource evidence is not a valid P0 result.
The result schema must be `zero-sdr-p0-result-v2` and record the actual resource
sampling interval.

The expected RF input is 2.048 million complex samples/s at two unsigned bytes per sample, or 4,096,000 bytes/s. A 16,384-byte input block represents 8,192 complex samples and 4,000 µs of RF time, so average spectrum plus demodulation work must stay below 4,000 µs/block. Unmuted WFM output should approach 32,000 mono frames/s. The automated P0 evaluation requires:

- `uptime_ms` of at least 1,800,000;
- one connection attempt, one successful connection, and no retry wait;
- zero read errors and IQ byte rate within ±10% of 4,096,000 bytes/s;
- zero audio drops, recoveries, write errors, or open failures;
- unmuted audio write rate within ±10% of 32,000 frames/s;
- average processing time below 4,000 µs per IQ block.

Retain `max_processing_us` and `max_ui_gap_ms` in the evidence. They expose scheduler spikes and UI stalls, but their final release thresholds must be derived from the physical Cardputer Zero run rather than a desktop simulator. Record process CPU and resident memory alongside these internal metrics because the app intentionally does not guess platform-wide resource figures.

## Evidence to attach to a release

- Device model/OS image and kernel version
- Dongle tuner/USB ID and whether external power was used
- `.deb` checksum
- complete `cardputerzero-sdr-p0` evidence directory and its `summarize_diagnostics.py --p0` PASS output
- Photos or native screenshots of both pages
- Completed table above with issue links for any waiver
