# Zero SDR for Cardputer Zero

[English](README.md) | [简体中文](README.zh-CN.md)

[![CI](https://github.com/k1412/cardputerzero-sdr/actions/workflows/ci.yml/badge.svg)](https://github.com/k1412/cardputerzero-sdr/actions/workflows/ci.yml)
[![Device Package](https://github.com/k1412/cardputerzero-sdr/actions/workflows/device-package.yml/badge.svg)](https://github.com/k1412/cardputerzero-sdr/actions/workflows/device-package.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-35dcc8.svg)](LICENSE)

Zero SDR is an open-source, keyboard-first RTL-SDR receiver for Cardputer Zero. The UI is rendered at the device's native 320×170 resolution and is designed around its physical `F/X/Z/C`, Enter, and Esc controls.

![Radio spectrum and waterfall](screenshots/radio-dark.png)

> [!IMPORTANT]
> This repository is a pre-release hardware bring-up project. The UI, deterministic offline demo, ten-language catalog, and desktop tests work. Live RTL-SDR capture, WFM audio, USB power behavior, and performance have **not** yet been verified on a physical Cardputer Zero, so no App Store release is claimed yet.

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
- Narrow `plugdev`/`uaccess` rules for the tested Realtek RTL2832U USB IDs; no root service
- Ten UI languages: English, Simplified Chinese, Traditional Chinese, Spanish, Japanese, Korean, French, German, Brazilian Portuguese, and Russian
- Linux/macOS/Windows desktop simulator support inherited from the official CardputerZero template
- Host tests for tuning, FFT/WFM DSP, runtime-loaded RTL-SDR and ALSA boundaries, reconnect behavior, settings persistence, and translation completeness

Not implemented or verified yet:

- RTL-SDR USB capture and audio on physical Cardputer Zero hardware
- ARM64 `.deb` installation on real hardware
- CardputerZero Store submission

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

The bottom bar always shows the primary actions. Z/C frequency tuning and F/X settings-row movement support controlled key repeat; step changes, toggles, confirmation, and direct entry execute once per press. Destructive or hidden key chords are intentionally absent.

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

Run 50 screenshot checks—radio, settings, direct tuning, and both radio/settings audio-failure states in all ten locales—with `scripts/smoke_locales.sh`. To capture only the direct-tuning overlay, start on the radio page and set `ZERO_SDR_DIRECT_ENTRY=103.9`.

Generate six reproducible receiver states—offline demo, fake-device `LIVE`, device-reported manual gain, `NO AUDIO`, direct tuning, and `NO DEVICE`—with `scripts/capture_demo_states.sh`.

## Runtime diagnostics

The app writes one structured `diagnostics` record to standard output every 30 seconds. Each record contains cumulative connection/retry/read-error counts, IQ blocks and bytes, audio generated/written/dropped frames, ALSA recovery/failure counts, DSP processing time, UI-loop count, and maximum UI-loop gap. Set `ZERO_SDR_DIAGNOSTICS_INTERVAL_MS` to a value from 100 to 3,600,000 when a different interval is needed.

The ARM64 package installs `cardputerzero-sdr-p0`, a non-root preflight and evidence runner. Do not launch a second Zero SDR instance from APPLaunch at the same time. From an SSH or local shell as the normal APPLaunch user, first verify framebuffer, keyboard, RTL-SDR USB-node, and ALSA access:

```sh
cardputerzero-sdr-p0 --preflight-only
```

Then run the bounded 30-minute session and use the physical controls normally while it is active:

```sh
cardputerzero-sdr-p0 --duration 1800
```

The runner creates a private evidence directory below `$XDG_STATE_HOME/cardputerzero-sdr/evidence/` (or `~/.local/state/`). It stores a sanitized hardware/access snapshot, `app.log`, process CPU/memory/temperature samples, and the exit result. It refuses root, excludes hostname/network state/USB serials, and stops the app through its tested clean `SIGTERM` path. Copy the evidence directory to a development checkout, then summarize the app log on the development machine:

```sh
python3 scripts/summarize_diagnostics.py --p0 path/to/evidence/app.log
```

The `--p0` check requires at least 30 minutes, one uninterrupted connection, zero RF/audio errors or drops, IQ throughput within 10% of 4,096,000 bytes/s, unmuted audio output within 10% of 32,000 frames/s, and average processing below the 4,000 µs IQ-block budget. Maximum processing and UI-loop gaps are reported for the physical-device baseline but are not guessed release thresholds. See [the hardware test plan](docs/DEVICE_TEST_PLAN.md) for the complete procedure.

## Cardputer Zero build

The cross preset follows the official CardputerZero CMake template and downloads its pinned BSP into `.cache/` when needed. An AArch64 cross compiler must be available.

```sh
cmake --workflow --preset cp0-cross-package
```

Expected package name: `dist/cardputerzero-sdr_0.1.0-1_arm64.deb`.

Do not treat a successful cross-build as device validation. Complete [the hardware test plan](docs/DEVICE_TEST_PLAN.md) before publishing a release.

## Project map

```text
src/app/       lifecycle, assets, simulator, screen switching
src/audio/     bounded runtime-loaded ALSA output
src/device/    RTL-SDR discovery/capture and receiver worker lifetime
src/dsp/       deterministic demo, FFT spectrum, channel filter, WFM demodulation
src/i18n/      locale catalog and font selection
src/model/     bounded radio state
src/platform/  SDL/DRM display and keyboard/evdev adapters
src/view/      LVGL screens, widgets, and 320×170 theme
src/viewmodel/ state formatting and UI actions
tests/         host-runnable unit tests
docs/          architecture, UX, language, and device validation guides
```

See [Architecture](docs/ARCHITECTURE.md), [UX and controls](docs/UX.md), [Internationalization](docs/I18N.md), and [Contributing](CONTRIBUTING.md).

## Store release policy

`app-builder.json` is prepared for the CardputerZero tooling, and all referenced screenshots are native 320×170 PNGs. Publication is held until the P0 hardware checks pass. The app does not install a root system service and does not require network or cloud access. Its udev rules grant only the tested Realtek `0bda:2832` and `0bda:2838` IDs to Debian's `plugdev` group/active-seat ACL; real-device permission behavior remains a P0 check.

The package name is `cardputerzero-sdr`. An existing Store app named `zerosdr` is a separate project; this repository does not claim compatibility or ownership of it.

## License

Application source code is MIT licensed. Device packages bundle librtlsdr 2.0.3 as a separate GPL-2.0-or-later shared library because it is absent from the current official BSP; its complete license is installed with the package. Bundled fonts and icon-font assets have their own notices under `assets/fonts/`. The generated Zero SDR app icon is contributed to this project under the repository's MIT license.
