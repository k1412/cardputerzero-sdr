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
       (implemented)      (implemented;
                         device unverified)
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

The live implementation keeps librtlsdr and ALSA handles out of UI code:

```cpp
RtlSdrDevice             // discover, open, tune, gain, read IQ blocks
RadioSession             // reconnect and worker lifetime
IqSpectrum               // fixed-size normalized spectrum frames
WfmDemodulator           // channel filter, FM discriminator, 32 kHz mono PCM
AlsaAudioSink            // bounded PCM queue and mute control
```

The receiver worker must never call LVGL. It publishes bounded frames to the main thread, drops stale visualization frames under load, and preserves audio continuity ahead of waterfall refresh rate. The main loop polls atomic receiver/audio state and publishes translated UI subjects, including audio-only failure while RF remains live. The WFM path uses a 100 kHz complex FIR before 4× RF decimation, performs FM discrimination at 512 kHz, averages down 4× to 128 kHz, and applies a 127-tap 15 kHz low-pass FIR before the final 4× conversion to 32 kHz mono PCM. The staged path rejects adjacent channels without a large intermediate buffer and prevents the 19 kHz stereo pilot from aliasing to an audible 13 kHz tone. A 30 Hz DC blocker removes tuner/crystal offset after de-emphasis before PCM conversion.

## Threading and memory rules

- LVGL objects are created and mutated only on the main UI thread.
- USB reads and demodulation must run outside the UI loop.
- Use bounded queues; never accumulate unbounded IQ, FFT, or PCM buffers.
- The waterfall stores only the visible 316×19 RGB565 pixels.
- The desktop LVGL software renderer is intentionally single-worker because concurrent FreeType access was unstable with CJK font rasterization.
- Device builds must be profiled before increasing FFT size or frame rate.

## Dependency policy

LVGL is pinned to 9.5.0. Fetched libraries use immutable release tags or hashed release archives. RTL-SDR and ALSA use runtime discovery so the app still opens cleanly when a dongle or audio route is absent. Cardputer packages bundle pinned librtlsdr 2.0.3 privately and use the BSP's declared libusb and ALSA runtimes. A narrow udev rule covers only Realtek `0bda:2832/2838`, matching Debian's `plugdev` convention while adding active-seat `uaccess`; it does not run the app as root.
