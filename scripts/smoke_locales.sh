#!/usr/bin/env bash
set -euo pipefail

binary="${1:-./build/linux-x86-64/Debug/cardputerzero-sdr}"
build_dir="$(cd -- "$(dirname -- "$binary")" && pwd)"
output_dir="${2:-/tmp/zero-sdr-locale-smoke}"
locales=(en zh-CN zh-TW es ja ko fr de pt-BR ru)
fake_rtlsdr="$build_dir/fake_rtlsdr.so"

mkdir -p "$output_dir"

for locale in "${locales[@]}"; do
  for page in radio settings direct-tune no-audio-radio no-audio-settings; do
    page_env=()
    if [[ "$page" == "settings" ]]; then
      page_env=(ZERO_SDR_START_PAGE=settings)
    elif [[ "$page" == "direct-tune" ]]; then
      page_env=(ZERO_SDR_DIRECT_ENTRY=103.9)
    elif [[ "$page" == "no-audio-radio" ]]; then
      page_env=(ZERO_SDR_LIVE=1
                ZERO_SDR_RTLSDR_LIBRARY="$fake_rtlsdr"
                ZERO_SDR_ALSA_LIBRARY="$output_dir/missing-libasound.so")
    elif [[ "$page" == "no-audio-settings" ]]; then
      page_env=(ZERO_SDR_LIVE=1
                ZERO_SDR_START_PAGE=settings
                ZERO_SDR_RTLSDR_LIBRARY="$fake_rtlsdr"
                ZERO_SDR_ALSA_LIBRARY="$output_dir/missing-libasound.so")
    fi
    env SDL_VIDEODRIVER=dummy \
      ZERO_SDR_LOCALE="$locale" \
      "${page_env[@]}" \
      ZERO_SDR_SCREENSHOT="$output_dir/$locale-$page.png" \
      ZERO_SDR_SCREENSHOT_EXIT=1 \
      "$binary" >"$output_dir/$locale-$page.log" 2>&1
    printf 'PASS %-5s %s\n' "$locale" "$page"
  done
done
