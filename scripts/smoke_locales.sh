#!/usr/bin/env bash
set -euo pipefail

binary="${1:-./build/linux-x86-64/Debug/cardputerzero-sdr}"
output_dir="${2:-/tmp/zero-sdr-locale-smoke}"
locales=(en zh-CN zh-TW es ja ko fr de pt-BR ru)

mkdir -p "$output_dir"

for locale in "${locales[@]}"; do
  for page in radio settings; do
    page_env=()
    if [[ "$page" == "settings" ]]; then
      page_env=(ZERO_SDR_START_PAGE=settings)
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
