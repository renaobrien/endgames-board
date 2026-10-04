# Build Guide

This guide follows one reference build. Each step says what I used, then what changes if you use something else.

## 1. Display and controller

**What I used:** Elecrow CrowPanel Advanced 5" ESP32-P4 HMI. 800x480 IPS color touchscreen, Wi-Fi 6 through its onboard ESP32-C6, USB-C power, battery connector with charging. About $43. Everything is on one board, so there's no wiring.

**If you use something else:**

- **The 7" CrowPanel Advanced (ESP32-P4, 1024x600):** same chip family, so the firmware should carry over with a new screen layout. The case needs a bigger shell.
- **An ESP32-S3 CrowPanel or another ESP32 touchscreen:** works, with a different display and touch driver in the firmware config. The S3 has Wi-Fi built in, so the C6 co-processor setup goes away.
- **A different screen size:** the case is sized to the reference board, so adjust the screen opening and mounting points in the source CAD.
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
