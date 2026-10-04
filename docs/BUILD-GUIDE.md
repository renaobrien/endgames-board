# Build Guide

Build a touchscreen chess board that plays your live games from [endgam.es](https://endgam.es), with your own piece set on it.

Each step says what I used, then what changes if you use something else.

> **Status:** the case and parts below are being test-printed on the reference board. The firmware is written but not yet running on real hardware. This guide gets updated as each part is confirmed.

## What you need

| Part | What I used | Notes |
|---|---|---|
| Screen + controller | Elecrow CrowPanel Advanced 5" ESP32-P4 HMI (SKU DHE04005D), 800x480 IPS touch, about $43 | Screen, touch, Wi-Fi and USB-C all on one board. No wiring |
| Power | USB-C cable and a 5V/2A USB power adapter | Plugs into the board's lower USB-C port |
| Screws | 4 x M3 x 20 mm | Hold the case together through the board's corner holes |
| Printer + filament | Any FDM printer, PLA | Nothing needs supports |
| Computer | Mac, Windows or Linux with a USB-C cable | Only for installing the firmware once |

**If you use something else:**

- **The 7" CrowPanel Advanced (ESP32-P4, 1024x600):** same chip family, so the firmware should carry over with a new screen layout. The case needs new dimensions (see `hardware/case/brick.py`).
- **An ESP32-S3 CrowPanel or another ESP32 touchscreen:** works with a different display and touch setup in `firmware/main/bsp.c`. The case won't fit.
- **A battery:** the board has a battery connector with charging, but this case has no battery space yet.

## 1. Print the test template first

The template is a thin frame that checks the board lines up with the case before you print the whole thing. It also prints the power slider extender.

1. Print `hardware/template.stl`.
   - Flat, no supports.
2. Push the extender's square socket onto the board's power switch nub.
   - The switch is labeled POWER, on the back, next to the USB-C ports.
3. Lay the board screen down on the template.
   - USB-C edge against the template's wall.
   - The edge with the UART1 and I2C connectors goes on the side with the notch.
4. Check:
   - The 4 corner holes line up with the board's holes.
   - The lower USB-C port shows through its window.
   - The extender's tab sticks out through the long slot and switches the board on and off.

**If something is off:** change the numbers at the top of `hardware/template.py` and `hardware/case/brick.py`, then re-run them to regenerate the files. Measurements I used are in `hardware/MEASUREMENTS.md`.

## 2. Install the firmware

Do this with the board out of the case. The case keeps the flashing port covered.

1. Install ESP-IDF on your computer.
   - Follow Espressif's ESP-IDF install guide for your system.
2. Plug the board into your computer.
   - Use the upper USB-C port, labeled UART0.
3. Build and flash.
   - In a terminal, from the `firmware` folder:
   - `idf.py set-target esp32p4`
   - `idf.py build flash monitor`
4. Watch the screen.
   - It should show the Wi-Fi setup screen.

**If it won't flash:** hold the BOOT button on the back, tap RESET, release BOOT, then flash again.

## 3. Print and assemble the case

The case is a flat brick, 138 x 85 x 22 mm, in two parts.

1. Print the parts.
   - `hardware/case/case-tray.stl`: floor down, no supports.
   - `hardware/case/case-lid.stl`: face down, no supports.
   - `hardware/case/power-slider-extender.stl` (or reuse the one from the template print).
2. Fit the extender.
   - Push it onto the power switch nub.
   - Slide the switch to ON.
3. Put the board in the tray.
   - Screen up.
   - The extender's tab goes through the slot in the side wall.
4. Close it.
   - Lay the lid on top.
   - Screw the 4 M3 x 20 mm screws in from the back.
5. Plug in power.
   - USB-C cable into the port on the side.

## 4. Connect and pair

1. Connect to Wi-Fi.
   - Follow the Wi-Fi setup screen on the board.
2. Pair it with your account.
   - The board shows a 6-letter code.
   - On your phone or computer, go to endgam.es/board/pair and sign in.
   - Type the code.
3. Play.
   - The board picks up your current game.
   - Tap a piece, then tap where it goes.

You can switch between the board and the website mid-game. Both show the same game.

## Troubleshooting

| Problem | Fix |
|---|---|
| Board shows "No game in progress" | Start a game on endgam.es. Games against the computer only play on the website |
| Pairing code expired | Codes last 10 minutes. Turn the board off and on for a new one |
| Board stopped showing your games | It was unpaired. It goes back to the pairing screen; pair again |
| Moves take a few seconds to appear | The board checks for new moves every few seconds |
