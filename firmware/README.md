# Firmware

Runs on the Elecrow CrowPanel Advanced 5" ESP32-P4 (800x480, GT911 touch). ESP-IDF + LVGL 9. Wi-Fi comes through the onboard ESP32-C6 (ESP-Hosted).

**Status:** the UI, API client, pairing flow and storage are written. The display, touch and Wi-Fi bring-up in `main/bsp.c` is not written yet because it needs the real board (start from Elecrow's sample). Nothing here has been compiled for the ESP32-P4 or run on hardware. The desktop preview renders the real UI code, which is what has been checked.

## Layout

| Path | What |
|---|---|
| `main/ui_game.c` | Game screen: 480x480 board, 320 px panel, tap to move, promotion picker, flip, menu |
| `main/ui_screens.c` | Wi-Fi setup, pairing, idle screens |
| `main/chess_pos.c` | FEN parsing, check detection, legal-move lookup. No LVGL |
| `main/api.c` | Device API client (`../protocol/API.md`) |
| `main/pieces_store.c` | Downloads the 12 piece PNGs from `board-pieces` into PSRAM |
| `main/store.c` | Wi-Fi and device token in NVS |
| `main/bsp.c` | Hardware bring-up. The only hardware-specific file. Not implemented |
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
. $IDF_PATH/export.sh
idf.py set-target esp32p4
idf.py build flash monitor
```

Never commit Wi-Fi passwords or the device token. `main/secrets.h` and `.env*` are gitignored.
