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
| Display | Inspect radio/settings in dark and light themes | no clipping, tearing, overlays, or unreadable text | Pending device |
| Keys | Exercise F/X/Z/C, Enter, Esc, G/M/L/T, number row, period, Backspace, and hold-to-repeat | one action per press; repeat is controlled; Esc returns safely | Pending device |
| USB discovery | Cold boot, hot-plug, unplug/replug RTL2832U | state changes without crash; UI stays responsive | Pending device |
| USB power | Capture continuously with screen/audio active | no brownout, disconnect loop, or thermal warning | Pending device |
| Tuning | Test 22.0, 97.4, and 948.6 MHz by stepping and direct entry; try 21.999/948.601 | valid values tune exactly; invalid values are rejected; no overflow/wrap | Pending device |
| RF capture | Run 2.048 MS/s for 30 minutes | no sustained overruns; reconnect path works | Pending device |
| WFM audio | Receive a known station, mute/unmute, unplug device | intelligible audio; no loud transient; mute is immediate | Pending device |
| Performance | Record CPU, memory, UI frame cadence, audio underruns | thresholds documented from real measurements | Pending device |
| Locales | Open radio/settings in all ten languages | glyphs render; no clipping or crash | Pending device |
| Persistence | Change frequency/step/gain/mute/theme/language, restart app | all survive and app can recover from malformed config | Pending device |

## Failure injection

- Start with no dongle.
- Remove the dongle during a USB read.
- Deny access to the USB device and input event node.
- Supply a malformed config file.
- Fill or make the user config directory read-only.
- Force the SDR worker behind real time and verify bounded frame dropping.
- Stop audio output and verify the receiver/UI can shut down cleanly.

## Evidence to attach to a release

- Device model/OS image and kernel version
- Dongle tuner/USB ID and whether external power was used
- `.deb` checksum
- 30-minute capture log and resource measurements
- Photos or native screenshots of both pages
- Completed table above with issue links for any waiver
