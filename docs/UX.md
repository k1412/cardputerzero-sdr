# Cardputer Zero UX

## Physical interaction model

The interface follows the launcher convention: `F/X/Z/C` are up/down/left/right, Enter confirms, and Esc backs out. Arrow keys mirror those controls in the desktop simulator.

The radio screen makes the most common action—frequency tuning—available without opening a menu. Up/down changes the tuning step rather than frequency so accidental vertical presses cannot retune. Key repeat is accepted only for left/right frequency tuning and up/down settings-row movement; step changes, toggles, confirmation, and direct entry execute once per physical press. Pressing a number opens direct MHz entry; the modal accepts up to three fractional digits, Enter commits, Backspace edits, and Esc cancels without retuning.

## 320×170 layout budget

| Region | Height | Purpose |
| --- | ---: | --- |
| Status/title bar | 24 px | launcher-consistent title and system status |
| Main content | 120 px | frequency, state, spectrum, waterfall/settings |
| Key-hint bar | 26 px | three discoverable physical-key actions |

Long device states use ellipsis. Settings values have a fixed right-aligned column. No page scroll is required for normal use.

## Boundary behavior

- Frequency is clamped to 22.0–948.6 MHz, the measured range of the target FC0012 sample dongle.
- Tuning never wraps from the upper bound to the lower bound.
- Direct entry rejects malformed or out-of-range values instead of silently clamping them.
- Step selection wraps because it is a small, reversible list.
- Manual gain cycles through the connected tuner's reported discrete values, with `AUTO` as an adjacent reversible choice; arbitrary unsupported dB values are never sent to hardware.
- Settings row selection wraps for fast one-handed use.
- Esc from settings returns to radio; Esc from radio exits to the launcher.
- Linux `EV_KEY=2` repeats are ignored for Enter/Esc, step selection, direct entry, and G/M/L/T toggles so a held key cannot oscillate pages or values.
- Missing hardware must show `NO DEVICE` while keeping settings and demo behavior usable.
- USB permission failure must show a localized access state and replug guidance so a stale pre-install device node can recover after the Store installs `librtlsdr0`.
- A device claimed by another process must show a localized busy state and tell the user to close the other SDR application.
- Audio initialization or playback failure must show `NO AUDIO` while preserving live RF, spectrum, settings, and keyboard control.
- USB or DSP failure must not freeze input or leave loud/stale audio playing.

## Accessibility and localization

The dark palette maintains high contrast and reserves amber/red for mode and warning states. Meaning is not conveyed by color alone: `DEMO`, `LIVE`, mute, `NO AUDIO`, USB-access recovery, device-busy recovery, and device errors are textual. Locale-specific fonts are bundled so CJK and Cyrillic rendering does not depend on the system image.
