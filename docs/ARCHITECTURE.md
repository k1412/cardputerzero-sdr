# Architecture

## Design goals

Zero SDR keeps the UI responsive on a small Linux device while hardware I/O and DSP evolve independently. Device-specific assumptions are isolated behind the platform layer; model and DSP code remain host-testable without LVGL or an SDR dongle.

## Data flow

```text
evdev / SDL keys
       |
       v
RadioModel <-> BaseViewModel -> LVGL subjects -> Radio / Settings screens
                    |
                    v
           Spectrum source interface
                    |
            +-------+--------+
            |                |
      synthetic demo     RTL-SDR pipeline
       (implemented)       (planned)
```

The current `SyntheticSpectrum` is deterministic. It exercises the exact chart and waterfall rendering path that live samples will use, but it is not radio data.

## Layer responsibilities

- `model`: frequency, step, gain, mute, theme, page, source, and locale state. Frequency limits are enforced here so every input path shares the same safety boundary.
- `viewmodel`: converts typed state into compact display strings and LVGL subjects.
- `view`: owns LVGL objects only. It is fixed to 320×170 and does not access USB or audio.
- `platform`: normalizes SDL and Linux evdev keys. RTL-SDR discovery/capture and audio output belong here behind narrow interfaces.
- `dsp`: transforms sample blocks into display/audio frames. Algorithms here must be testable from recorded or synthetic vectors.
- `app`: initializes LVGL/display, loads configuration, selects desktop/device backends, and owns screen lifetime.

## Live receiver boundary

The live implementation should introduce these interfaces without leaking librtlsdr handles into UI code:

```cpp
struct RadioDeviceInfo;
class IRadioSource;      // discover, open, tune, gain, read IQ blocks
class ISpectrumSink;     // fixed-size normalized spectrum frames
class IAudioSink;        // bounded PCM queue and mute control
```

The receiver worker must never call LVGL. It publishes bounded frames to the main thread, drops stale visualization frames under load, and preserves audio continuity ahead of waterfall refresh rate.

## Threading and memory rules

- LVGL objects are created and mutated only on the main UI thread.
- USB reads and demodulation must run outside the UI loop.
- Use bounded queues; never accumulate unbounded IQ, FFT, or PCM buffers.
- The waterfall stores only the visible 316×19 RGB565 pixels.
- The desktop LVGL software renderer is intentionally single-worker because concurrent FreeType access was unstable with CJK font rasterization.
- Device builds must be profiled before increasing FFT size or frame rate.

## Dependency policy

LVGL is pinned to 9.5.0. Fetched libraries use immutable release tags. Live RTL-SDR support should prefer runtime discovery or a packaged, declared dependency so the demo still opens cleanly when no dongle is attached.
