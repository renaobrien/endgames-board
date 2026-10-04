#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Converts the OFL fonts in fonts/src to LVGL C fonts in main/fonts.
# Needs Node. Run from firmware/: ./fonts/build-fonts.sh
set -euo pipefail
cd "$(dirname "$0")"
OUT=../main/fonts
RANGE="0x20-0x7E,0xB7,0x2026"   # ASCII, middle dot, ellipsis

conv() { # name file size
  npx --yes lv_font_conv --bpp 4 --size "$3" --font "src/$2" -r "$RANGE" \
    --format lvgl --lv-font-name "$1" --no-compress -o "$OUT/$1.c"
}

conv eg_sora_16      Sora-Regular.ttf   16
conv eg_sora_20      Sora-Regular.ttf   20
conv eg_sora_20_bold Sora-SemiBold.ttf  20
conv eg_vt323_36     VT323-Regular.ttf  36
conv eg_vt323_56     VT323-Regular.ttf  56
conv eg_bungee_28    Bungee-Regular.ttf 28
conv eg_bungee_44    Bungee-Regular.ttf 44
