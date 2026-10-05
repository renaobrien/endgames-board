# Firmware

Runs on the Elecrow CrowPanel Advanced 5" ESP32-P4 (800x480, GT911 touch). ESP-IDF + LVGL 9. Wi-Fi comes through the onboard ESP32-C6 (ESP-Hosted).

**Status:** compiles for the ESP32-P4 with ESP-IDF v5.5.1 (verified 2026-10-04). `main/bsp.c` brings up the screen, touch, backlight and Wi-Fi using the pins and timings from Elecrow's published sources for this board. Runs on the real board; over-the-air releases 0.8.0 to 0.10.1 have shipped and one board is paired.

## Layout

| Path | What |
|---|---|
| `main/ui_game.c` | Game screen: 480x480 board, 320 px panel, tap to move, promotion picker, flip, menu |
| `main/ui_screens.c` | Wi-Fi setup, pairing, idle screens |
| `main/chess_pos.c` | FEN parsing, check detection, legal-move lookup. No LVGL |
| `main/api.c` | Device API client (`../protocol/API.md`) |
| `main/pieces_store.c` | Downloads the 12 piece PNGs from `board-pieces` into PSRAM |
| `main/store.c` | Wi-Fi and device token in NVS |
| `main/bsp.c` | Hardware bring-up: power rails, I2C, backlight MCU, 800x480 RGB panel, GT911 touch, Wi-Fi via the C6. The only hardware-specific file |
| `main/theme.h` | Generated colors. Do not edit |
| `main/fonts/` | Generated LVGL fonts (Sora, VT323, Bungee, all OFL) |
| `tools/preview/` | Desktop renderer for the screens |

## Colors and pieces come from the website

- `theme.h` is generated from the website's `src/styles/v2.css` by `npm run board:theme` in the endgames repo. Never hand-copy colors.
- Pieces come from the `board-pieces` endpoint, which resizes the same art the website draws.
- Fonts: `./fonts/build-fonts.sh` converts the TTFs in `fonts/src` (needs Node). OFL license files sit next to them.

## Preview on a desktop

```
cd firmware/tools/preview
./render.sh
```

Needs cmake, a C compiler and git. Writes PNGs to `docs/screenshots/`. The 12 piece PNGs in `tools/preview/pieces/` come from `npm run board:preview-pieces` (default set only).

## Build for the board

```
. $IDF_PATH/export.sh          # ESP-IDF v5.5.x
idf.py set-target esp32p4
idf.py build flash monitor     # board plugged into the UART0 USB-C
```

Versions that build: ESP-IDF v5.5.1, LVGL 9.2.2, esp_lvgl_port 2.6.x (newer esp_lvgl_port needs a newer IDF and LVGL).

Never commit Wi-Fi passwords or the device token. `main/secrets.h` and `.env*` are gitignored.
