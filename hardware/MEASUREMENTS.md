# Reference board measurements

Board: Elecrow CrowPanel Advanced 5" ESP32-P4 HMI, SKU DHE04005D, PCB marked "ESP32 P4-Advance HMI Display 5.0 V1.0".

Measured by hand with calipers (2026-10-04) unless marked otherwise. "Back view" = looking at the PCB side, screen face down.

| What | Value | Source |
|---|---|---|
| Length (long edge) | 131 mm (5.16") | Elecrow spec; caliper photo agrees within about 1 mm |
| Width (short edge) | 80.2 mm (3.158") | Caliper |
| Thickness, screen + PCB | 5.1 mm (0.20") | Caliper |
| Thickness, deepest part (no battery) | 16.5 mm (0.65") | Caliper. On the microSD edge; from the photo this is the 2x8 pin header |
| Visible screen glass, long side | 119 mm (4.7") | Caliper |
| PCB lip around the glass | about 5 mm (0.2") per side | Caliper |
| Screen glass, short side | 75.8 mm (2.9845") | Caliper |
| Black border on the glass, top | 2.5 mm (0.1") | Caliper |
| Black border on the glass, bottom | 7.6 mm (0.3") | Caliper. On the microSD edge. Wider side is where the display's ribbon cable enters |
| Black border on the glass, left and right | 5.1 mm (0.2") each | Caliper |
| Lit display area (derived) | about 109 x 66 mm | Glass minus borders. Matches Elecrow's 108 x 65 mm spec |
| Mounting holes | 4, one per corner, on the PCB lip outside the glass | Visual |
| Hole diameter | 2.5 mm (0.10"), fits M2.5 screws | Caliper |
| Hole center from edges | about 3.5 mm from both edges | Estimated from photo. Confirm with the test-fit plate |

## From the side photo of the USB-C edge

- Order along that edge, from the 5V-in end: white UART3-IN connector, USB-C (USB2.0), USB-C (UART0), power slider.
- USB-C shells sit about 1.8 mm off the board and overhang the edge about 1 mm. The case and template allow 1.6 mm on that side.
- The white UART3-IN connector stays inside the case (Rena confirmed it does not stick out past the edge).

## Things on the edges (back view, USB ports on the right)

| Edge | Item | Case needs |
|---|---|---|
| Right | USB-C "UART0" (upper), used for flashing/serial | Cutout |
| Right | USB-C "USB2.0" (lower) | Cutout |
| Right | UART3-IN connector (5V in), below the USB ports | Cutout or leave closed |
| Top right | POWER slide switch | Cutout so it can be switched |
| Bottom left | microSD (TF) slot | Cutout, optional |
| Left | BOOT and RESET buttons (on the back, near the left edge) | Access holes, needed for flashing |
| Inset, bottom | BAT, SPKR connectors | No cutout; cable routes inside the case |
| Front | Status LEDs CHG/PWR near the USB ports | Optional light pipe |

## Front window

The lit area is not centered top to bottom: it sits about 2.5 mm away from the microSD edge (toward the UART1/I2C edge). The case's front window follows the lit area, not the glass, or one edge shows a fat black strip.

## Open


- Hole-center positions: print the test-fit plate (`hardware/test-fit-plate.scad`) and check the screws line up before printing the full case.

## Power slider (caliper)

- Switch body 7.62 x 3.68 x 5.13 mm (0.300 x 0.145 x 0.202"), set 3.49 mm (0.1375") in from the USB-C edge.
- Nub 1.5 mm (0.059") square, pointing straight out from the back. Its tip is 5.1 mm (0.202") from the board (confirmed by side photo).
- Travel not measured; the case slot allows the most the nub can move inside its body (6.1 mm).
- Handled with a printed extender (`case/power-slider-extender.stl`). The button wiring below is no longer needed.

## Power button (not used)

- Push button (red + black wires, white cap) between header pin **IO29** and **GND**. Firmware: internal pull-up, press = backlight off + light sleep, press again = wake.
- Header pins free for general use: IO29, IO30, IO31. Not free: IO26 (SPI clock), IO32 (resets the Wi-Fi chip), IO47/IO48 (shared with UART1 / wireless socket).
- Power slider stays ON permanently.
- No-solder wiring: female Dupont jumper leads + Wago 221-412 lever connectors.
- Source: espboards.dev pinout for the CrowPanel Advance 5.0 (ESP32-P4).
