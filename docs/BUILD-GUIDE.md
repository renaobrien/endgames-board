# Build Guide

This guide follows one reference build. Each step says what I used, then what changes if you use something else.

## 1. Display and controller

**What I used:** Elecrow ESP32 display. Exact model and screen size are added here once the board is in hand.

**If you use something else:**

- **Another ESP32 with a separate e-paper panel:** works. You wire the panel yourself (see `electronics/`) and change the display driver in the firmware config.
- **A different screen size:** the case is sized to the reference board, so you'll need to adjust the case's screen opening and mounting holes in the source CAD.
- **A non-ESP32 controller:** the API in `protocol/API.md` is plain HTTPS and JSON, so any controller with Wi-Fi works. You'll need to port the firmware.

## 2. Case

Printable files and print settings land in `hardware/` once the case fits the reference board.

## 3. Firmware

Flashing steps land in `firmware/` with the first working build.

## 4. Pairing

1. Power on the board.
   - It connects to Wi-Fi.
   - It shows a 6-character code.
2. On your phone or computer, go to endgam.es/board/pair.
   - Sign in.
   - Enter the code.
3. Wait a few seconds.
   - The board shows your current game.
