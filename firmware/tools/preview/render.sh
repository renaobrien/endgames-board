#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds the preview and writes ../../../docs/screenshots/board-game.png (800x480).
# Needs: cmake, a C compiler, git (fetches LVGL), and Python 3 with Pillow or macOS sips.
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j >/dev/null
OUT=../../../docs/screenshots
mkdir -p "$OUT"
for SCREEN in ${SCREENS:-game pair idle wifi home quick quick-none find find-confirm}; do
  ./build/preview "$OUT/board-$SCREEN.bmp" $SCREEN
  if command -v sips >/dev/null; then sips -s format png "$OUT/board-$SCREEN.bmp" --out "$OUT/board-$SCREEN.png" >/dev/null
  else python3 -c "from PIL import Image; Image.open('$OUT/board-$SCREEN.bmp').save('$OUT/board-$SCREEN.png')"; fi
  rm "$OUT/board-$SCREEN.bmp"
  echo "$OUT/board-$SCREEN.png"
done
