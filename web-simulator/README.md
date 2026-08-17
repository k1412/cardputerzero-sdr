# Zero SDR browser product simulator

LAN-only browser simulator and radio for product-level review of the 320×170 Cardputer Zero application UI.

It mirrors the application's frequency limits, tuning steps, FC0012 gain values, ten locales, light/dark themes, keyboard policy, direct frequency entry, and release-relevant receiver failure states. When the optional native bridge is running, the simulator displays privacy-safe spectrum and metrics from the attached RTL-SDR and sends bounded tuning/gain controls to it. USB serials and host/network identity are never exposed by the API.

## Run locally

```sh
python3 -m http.server 8080 --directory web-simulator
```

Open <http://127.0.0.1:8080>.

## Container

```sh
docker build -t zero-sdr-simulator web-simulator
docker run --rm -p 8080:8080 zero-sdr-simulator
```

The container health endpoint is `/healthz`.

## Native RTL-SDR bridge

Build against the same device and DSP implementations used by the application:

```sh
mkdir -p build/web-simulator
g++ -std=c++20 -O2 -pthread -ldl -Isrc \
  web-simulator/bridge.cpp \
  src/device/rtl_sdr_device.cpp src/dsp/iq_spectrum.cpp \
  src/dsp/wfm_demodulator.cpp \
  -o build/web-simulator/zero-sdr-bridge
ZERO_SDR_WEB_ROOT="$PWD/web-simulator" \
  ./build/web-simulator/zero-sdr-bridge
```

The bridge listens on `0.0.0.0:18117` by default. It serves the static simulator, `GET /api/status`, `GET /api/audio`, bounded `POST /api/control`, and `/healthz`.

The live bridge carries spectrum data, bounded frequency/gain controls, and mono 32 kHz WFM audio. Open `http://<host-lan-ip>:18117`, select a frequency in the 76–108 MHz broadcast band, and press **播放真机声音**. Browsers require this click before audio may start. Audio is delivered as short same-origin signed 16-bit PCM blocks from `GET /api/audio`; the bridge keeps only three seconds in memory and does not record it.

The service is intended for a trusted LAN or private tailnet and is not exposed through a public reverse proxy. Control requests are globally limited to 30 requests per second and audio requests to 120 requests per second.

## Public design references

- [M5Stack Cardputer Zero product page](https://shop.m5stack.com/pages/m5-cardputerzero): enclosure, 1.9-inch display, USB-C/power placement, and 46-key keyboard.
- [Zero SDR public repository](https://github.com/k1412/cardputerzero-sdr): on-device UI, interaction model, and receiver behavior.
