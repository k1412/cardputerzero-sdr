# Zero SDR for Cardputer Zero

[English](README.md) | [简体中文](README.zh-CN.md)

[![CI](https://github.com/k1412/cardputerzero-sdr/actions/workflows/ci.yml/badge.svg)](https://github.com/k1412/cardputerzero-sdr/actions/workflows/ci.yml)
[![Device Package](https://github.com/k1412/cardputerzero-sdr/actions/workflows/device-package.yml/badge.svg)](https://github.com/k1412/cardputerzero-sdr/actions/workflows/device-package.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-35dcc8.svg)](LICENSE)

Zero SDR is an open-source, keyboard-first RTL-SDR receiver for Cardputer Zero. The UI is rendered at the device's native 320×170 resolution and is designed around its physical `F/X/Z/C`, Enter, and Esc controls.

![Radio spectrum and waterfall](screenshots/radio-dark.png)

> [!IMPORTANT]
> This repository is a pre-release hardware bring-up project. The UI, deterministic offline demo, ten-language catalog, and desktop tests work. Live RTL-SDR capture, WFM audio, USB power behavior, and performance have **not** yet been verified on a physical Cardputer Zero. [Store PR #116](https://github.com/CardputerZero/packages/pull/116) is therefore a draft used for independent CI validation, not a published App Store release.

## Current status

- Native 320×170 spectrum and waterfall UI
- Deterministic demo centered on measured FM stations
- Cardputer key mapping with repeat-aware tuning
- Direct MHz entry from the number row with decimal, backspace, Enter, and Esc handling
- Frequency clamp matching the tested FC0012 dongle: 22.0–948.6 MHz
- Six tuning steps from 10 kHz to 1 MHz
- Automatic gain plus device-reported manual gain steps, mute state, dark/light theme
- Last-used frequency, tuning step, gain, mute, theme, and language persist across launches
- Runtime-loaded RTL-SDR capture, channel-filtered WFM audio, and automatic reconnect
- Structured 30-second receiver-health logs for IQ, audio, DSP, reconnect, and UI-loop measurements
- Packaged non-root P0 preflight and bounded evidence runner with privacy-filtered hardware/resource facts
- Audited ARM64 Debian package with a private pinned librtlsdr runtime
- Debian-owned RTL-SDR `plugdev` permissions through `librtlsdr0`; no app-owned udev rule or root service
- Ten UI languages: English, Simplified Chinese, Traditional Chinese, Spanish, Japanese, Korean, French, German, Brazilian Portuguese, and Russian
- Linux/macOS/Windows desktop simulator support inherited from the official CardputerZero template
- Host tests for tuning, FFT/WFM DSP, runtime-loaded RTL-SDR and ALSA boundaries, reconnect behavior, settings persistence, and translation completeness

Not implemented or verified yet:

- RTL-SDR USB capture and audio on physical Cardputer Zero hardware
- ARM64 `.deb` installation on real hardware
- Physical P0 evidence and promotion of the draft CardputerZero Store submission to review/merge

Mutable settings are stored under `$XDG_CONFIG_HOME/cardputerzero-sdr/` or `~/.config/cardputerzero-sdr/`. The packaged `/etc/cardputerzero-sdr.conf` and repository default remain read-only fallbacks.

## Controls

| Key | Radio screen | Settings screen |
| --- | --- | --- |
| `F` / Up | Larger tuning step | Previous row |
| `X` / Down | Smaller tuning step | Next row |
| `Z` / Left | Tune down | Previous value |
| `C` / Right | Tune up | Next value |
| Enter | Open settings | Return to radio |
| Esc | Exit to launcher | Return to radio |
| `G` | Toggle auto/manual gain | Toggle gain mode |
| `M` | Mute/unmute | Mute/unmute |
| `L` | Next language | Next language |
| `T` | Dark/light theme | Dark/light theme |
| `0`–`9`, `.` | Open direct MHz entry | — |
| Backspace | Delete direct-entry character | — |

The bottom bar always shows the primary actions. Z/C frequency tuning and F/X settings-row movement support controlled key repeat; on the physical TCA8418 keyboard the app matches APPLaunch's 500 ms delay and 50 ms cadence in userspace because the device tree does not enable kernel autorepeat. Step changes, toggles, confirmation, and direct entry execute once per press. Destructive or hidden key chords are intentionally absent.

On the Settings gain row, Left/Right cycles `AUTO` and the exact gain values reported by the connected tuner. The FC0012 target exposes −9.9, −4.0, 7.1, 17.9, and 19.2 dB; other RTL-SDR tuners use their own reported list. `G` remains a one-press auto/manual shortcut.

## Desktop build

Requirements: CMake 3.31+, Ninja, Python 3.9+, a C++17 compiler, SDL2, FreeType, libpng, libjpeg, and zlib. `fmt` 12.1.0 is fetched when a compatible system package is unavailable.

On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build python3 libsdl2-dev \
  libfreetype-dev libpng-dev libjpeg-dev zlib1g-dev
cmake --preset linux-x86-64
cmake --build --preset linux-x86-64-dbg
ctest --test-dir build/linux-x86-64 -C Debug --output-on-failure
./build/linux-x86-64/Debug/cardputerzero-sdr
```

Headless screenshot smoke test:

```sh
SDL_VIDEODRIVER=dummy \
ZERO_SDR_LOCALE=zh-CN \
ZERO_SDR_START_PAGE=settings \
ZERO_SDR_SCREENSHOT=/tmp/zero-sdr.png \
ZERO_SDR_SCREENSHOT_EXIT=1 \
./build/linux-x86-64/Debug/cardputerzero-sdr
```

Run 60 screenshot checks—radio, settings, direct tuning, both radio/settings audio-failure states, and USB-access recovery in all ten locales—with `scripts/smoke_locales.sh`. To capture only the direct-tuning overlay, start on the radio page and set `ZERO_SDR_DIRECT_ENTRY=103.9`.

Generate eight reproducible receiver states—offline demo, fake-device `LIVE`, device-reported manual gain, `NO AUDIO`, direct tuning, `NO DEVICE`, USB access denied, and device busy—with `scripts/capture_demo_states.sh`.

## Runtime diagnostics

The app writes one structured `diagnostics` record to standard output every 30 seconds. Each record contains cumulative connection/retry/read-error counts, IQ blocks and bytes, audio generated/written/dropped frames, ALSA recovery/failure counts, DSP processing time, UI-loop count, and maximum UI-loop gap. Set `ZERO_SDR_DIAGNOSTICS_INTERVAL_MS` to a value from 100 to 3,600,000 when a different interval is needed.

The ARM64 package installs `cardputerzero-sdr-p0`, a non-root preflight and evidence runner. Do not leave another Zero SDR instance running. From an SSH or local shell as the normal APPLaunch user, first verify the device-tree model and official Cardputer Zero overlay, installed ARM64 app and `librtlsdr0` packages, `plugdev` membership, the distribution-owned RTL-SDR rule, a USB high-speed link, native 320×170 framebuffer access, the official APPLaunch/Cardputer keyboard path, RTL-SDR USB-node access, and a writable ALSA playback node:

```sh
cardputerzero-sdr-p0 --preflight-only
```

Both the application and preflight honor `LV_LINUX_FBDEV_DEVICE` and
`APPLAUNCH_LINUX_FBDEV_DEVICE` before falling back to `/dev/fb0`, matching the
official Launcher handoff and keeping redirected display tests faithful.

The preflight is read-only and reports whether `APPLaunch.service` is active. Then run the bounded 30-minute session and use the physical controls normally while it is active:

```sh
cardputerzero-sdr-p0 --duration 1800
```

For the full run, the tool pauses an active `APPLaunch.service` through the normal user's systemd manager before it starts Zero SDR, then restores the launcher on completion, interruption, or an early app exit. This matches the launcher's exclusive-framebuffer contract without root access; the result records both transitions. The runner also refuses to start while another Zero SDR process exists.

The runner creates a private evidence directory below `$XDG_STATE_HOME/cardputerzero-sdr/evidence/` (or `~/.local/state/`). It stores a sanitized hardware/access snapshot, `app.log`, process CPU/memory/temperature samples, and the exit result. It refuses root, excludes hostname/network state/USB serials, and stops the app through its tested clean `SIGTERM` path. Copy the evidence directory to a development checkout, then audit it on the development machine:

```sh
python3 scripts/summarize_diagnostics.py --p0 path/to/evidence
```

The `--p0` check audits the complete evidence directory rather than trusting `app.log` alone. It validates device-tree identity, the preflight schema and privacy boundary, installed packages, absence of a concurrent app, clean app exit, APPLaunch pause/restore, diagnostic counter monotonicity, resource-sample coverage, and the 30-minute capture gates. Those gates require one uninterrupted connection, zero RF/audio errors or drops, IQ throughput within 10% of 4,096,000 bytes/s, unmuted audio output within 10% of 32,000 frames/s, and average processing below the 4,000 µs IQ-block budget. Maximum processing and UI-loop gaps are reported for the physical-device baseline but are not guessed release thresholds. See [the hardware test plan](docs/DEVICE_TEST_PLAN.md) for the complete procedure.

## Cardputer Zero build

The cross preset follows the official CardputerZero CMake template and downloads its pinned BSP into `.cache/` when needed. An AArch64 cross compiler must be available.

```sh
cmake --workflow --preset cp0-cross-package
```

Expected package name: `dist/cardputerzero-sdr_0.1.0-6_arm64.deb`.

Do not treat a successful cross-build as device validation. Complete [the hardware test plan](docs/DEVICE_TEST_PLAN.md) before publishing a release.

## Project map

```text
src/app/       lifecycle, assets, simulator, screen switching
src/audio/     bounded runtime-loaded ALSA output
src/device/    RTL-SDR discovery/capture and receiver worker lifetime
src/dsp/       deterministic demo, FFT spectrum, channel filter, WFM demodulation
src/i18n/      locale catalog and font selection
src/model/     bounded radio state
src/platform/  framebuffer/DRM selection and keyboard/evdev adapters
src/view/      LVGL screens, widgets, and 320×170 theme
src/viewmodel/ state formatting and UI actions
tests/         host-runnable unit tests
docs/          architecture, UX, language, and device validation guides
```

See [Architecture](docs/ARCHITECTURE.md), [UX and controls](docs/UX.md), [Internationalization](docs/I18N.md), [Upstream compatibility](docs/UPSTREAM_COMPATIBILITY.md), and [Contributing](CONTRIBUTING.md).

## Store release policy

`app-builder.json` is prepared for the CardputerZero tooling, and all referenced screenshots are native 320×170 PNGs. [Draft Store PR #116](https://github.com/CardputerZero/packages/pull/116) exercises the official repository's validation without requesting publication; it must remain a draft until the P0 hardware checks pass and their evidence is attached. The app does not install a root system service, an application-owned system udev rule, or any maintainer script, and it does not require network or cloud access. The package depends on Debian's `librtlsdr0`, whose distribution-owned rule grants the supported RTL-SDR devices to `plugdev`; the current device Store resolves this dependency through Debian's package manager. Access by the normal APPLaunch user remains a physical P0 gate. Device-package CI also runs the pinned, digest-verified install-path policy from the authoritative Store repository against every `.deb`.

The package name is `cardputerzero-sdr`, its Store/APPLaunch title is `Zero SDR Keyboard`, and its explicit Store share code is `ZSDR`. The code avoids the registry generator's package-prefix fallback, which is already ambiguous for several `cardputerzero-*` listings. An existing Store app named `zerosdr` is a separate project; the distinct listing title prevents ambiguity, and this repository does not claim compatibility or ownership of it.

## License

Application source code is MIT licensed. Device packages bundle librtlsdr 2.0.3 as a separate GPL-2.0-or-later shared library because it is absent from the current official BSP; its complete license is installed with the package. Bundled fonts and icon-font assets have their own notices under `assets/fonts/`. The generated Zero SDR app icon is contributed to this project under the repository's MIT license.
