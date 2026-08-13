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
- Reproducible demo-state capture for offline, fake-live, manual-gain, direct-entry, missing-device, and audio-failure UX
- CardputerZero Store manifest, icon, screenshots, and release gates
- Runtime-loaded RTL-SDR device session with reconnect, live IQ spectrum, channel-filtered/DC-blocked WFM mono demodulation, stereo-pilot alias rejection, and bounded ALSA output
- Atomic persistence for frequency, tuning step, gain, mute, theme, and language with strict range validation
- Narrow udev permissions for Realtek RTL2832U `0bda:2832/2838` without a root service
- Graceful audio-only degradation with translated `NO AUDIO` feedback while RF and spectrum remain live
- Context-aware key-repeat policy that prevents held discrete keys from toggling values or pages repeatedly
- Device-reported discrete tuner gain profiles with nearest-value enforcement and full FC0012 negative-gain support
- XDG-scoped mutable settings so desktop use and screenshot automation never rewrite repository/package defaults
- Structured receiver-health telemetry plus an automated 30-minute P0 log summarizer
- Muted/unavailable-audio WFM bypass to preserve device CPU budget without stopping RF visualization

### Known limitations

- Live RTL-SDR capture, channel-filtered WFM audio, and automatic reconnect are implemented but not yet verified on physical Cardputer Zero hardware
- ARM64 package has not been installed on a physical Cardputer Zero
