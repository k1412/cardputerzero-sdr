#!/usr/bin/env bash
set -euo pipefail

binary="${1:-./build/linux-x86-64/Debug/cardputerzero-sdr}"
build_dir="$(cd -- "$(dirname -- "$binary")" && pwd)"
output_dir="${2:-/tmp/zero-sdr-demo-states}"
fake_rtlsdr="$build_dir/fake_rtlsdr.so"
fake_asound="$build_dir/fake_asound.so"
config_root="$(mktemp -d)"
trap 'rm -rf -- "$config_root"' EXIT

mkdir -p "$output_dir"

manual_gain_config="$config_root/manual-gain/cardputerzero-sdr/cardputerzero-sdr.conf"
mkdir -p "$(dirname -- "$manual_gain_config")"
sed -e 's/^automatic_gain=yes$/automatic_gain=no/' \
    -e 's/^gain_tenths_db=192$/gain_tenths_db=-40/' \
    config/cardputerzero-sdr.conf >"$manual_gain_config"

capture() {
  local name="$1"
  shift
  env SDL_VIDEODRIVER=dummy \
    XDG_CONFIG_HOME="$config_root/$name" \
    ZERO_SDR_SCREENSHOT="$output_dir/$name.png" \
    ZERO_SDR_SCREENSHOT_EXIT=1 \
    "$@" "$binary" >"$output_dir/$name.log" 2>&1
  printf 'PASS %s\n' "$name"
}

capture demo env ZERO_SDR_DEMO=1
capture direct-tune env ZERO_SDR_DEMO=1 ZERO_SDR_DIRECT_ENTRY=103.9
capture live env ZERO_SDR_LIVE=1 \
  ZERO_SDR_RTLSDR_LIBRARY="$fake_rtlsdr" \
  ZERO_SDR_ALSA_LIBRARY="$fake_asound"
capture manual-gain env ZERO_SDR_LIVE=1 \
  ZERO_SDR_START_PAGE=settings \
  ZERO_SDR_RTLSDR_LIBRARY="$fake_rtlsdr" \
  ZERO_SDR_ALSA_LIBRARY="$fake_asound"
capture live-no-audio env ZERO_SDR_LIVE=1 \
  ZERO_SDR_RTLSDR_LIBRARY="$fake_rtlsdr" \
  ZERO_SDR_ALSA_LIBRARY="$output_dir/missing-libasound.so"
capture no-device env ZERO_SDR_LIVE=1 \
  ZERO_SDR_RTLSDR_LIBRARY="$output_dir/missing-librtlsdr.so" \
  ZERO_SDR_ALSA_LIBRARY="$fake_asound"
