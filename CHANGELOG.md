# Changelog

All notable changes will be documented here. The project follows Semantic Versioning once the first device-validated release is published.

## Unreleased

### Added

- Native 320×170 radio, spectrum, waterfall, and settings screens
- Cardputer keyboard mapping and repeat-aware tuning
- Direct MHz frequency entry using the physical number row
- Deterministic offline spectrum demo
- Ten-language catalog with bundled CJK/Cyrillic font subsets
- Desktop screenshot automation and model/DSP/i18n tests
- Reproducible demo-state capture for offline, fake-live, manual-gain, direct-entry, missing-device, audio-failure, USB-access, and device-busy UX
- CardputerZero Store manifest, icon, screenshots, and release gates
- Runtime-loaded RTL-SDR device session with reconnect, live IQ spectrum, channel-filtered/DC-blocked WFM mono demodulation, stereo-pilot alias rejection, and bounded ALSA output
- Atomic persistence for frequency, tuning step, gain, mute, theme, and language with strict range validation
- Debian `librtlsdr0` runtime dependency for distribution-owned RTL-SDR USB permissions without a root service
- Graceful audio-only degradation with translated `NO AUDIO` feedback while RF and spectrum remain live
- Context-aware key-repeat policy that prevents held discrete keys from toggling values or pages repeatedly
- Device-reported discrete tuner gain profiles with nearest-value enforcement and full FC0012 negative-gain support
- XDG-scoped mutable settings so desktop use and screenshot automation never rewrite repository/package defaults
- Structured receiver-health telemetry plus an automated 30-minute P0 log summarizer
- Muted/unavailable-audio WFM bypass to preserve device CPU budget without stopping RF visualization
- Non-root Cardputer Zero P0 preflight/resource collector with privacy-filtered release evidence
- Graceful `SIGINT`/`SIGTERM` shutdown through the normal receiver and audio cleanup path
- APPLaunch-aware Cardputer keyboard discovery with strict full-control capability matching and single-device fallback
- APPLaunch-matched 500/50 ms userspace key repeat for the non-autorepeating TCA8418 driver
- APPLaunch/LVGL framebuffer-device overrides in both the application and P0 preflight, with the build-time `/dev/fb0` fallback retained
- SHA-256 verification for downloaded and cached copies of the official v0.0.4 Cardputer Zero BSP
- Store-compliant packaging with no application-owned system udev rule or maintainer scripts
- Device-package CI enforcement of the pinned authoritative Store install-path and archive-safety policy
- P0 package, `plugdev`, distro-udev, and USB high-speed gates with privacy-preserving evidence
- Localized USB-access recovery and device-busy guidance instead of a generic hardware error
- Stable compiler source paths and no transient CI/workspace path in the device binary
- Store metadata aligned with the registry-consumed permission object plus localized detail text for all ten app languages
- Non-root P0 pause/restore of the APPLaunch user service for exclusive framebuffer evidence, with concurrent-instance rejection
- Distinct `Zero SDR Keyboard` Store/APPLaunch listing title to avoid ambiguity with the existing third-party `zerosdr` package
- Whole-directory P0 evidence validation covering device-tree identity, preflight privacy/access, launcher restoration, clean exit, monotonic diagnostics, and resource-sample duration
- Draft official Store submission for independent package validation, explicitly held from review and merge until physical P0 evidence is attached

### Known limitations

- Live RTL-SDR capture, channel-filtered WFM audio, and automatic reconnect are implemented but not yet verified on physical Cardputer Zero hardware
- ARM64 package has not been installed on a physical Cardputer Zero
