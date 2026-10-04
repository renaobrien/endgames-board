# Reference board measurements

Board: Elecrow CrowPanel Advanced 5" ESP32-P4 HMI, SKU DHE04005D, PCB marked "ESP32 P4-Advance HMI Display 5.0 V1.0".

Measured by hand with calipers (2026-10-04) unless marked otherwise. "Back view" = looking at the PCB side, screen face down.

| What | Value | Source |
|---|---|---|
| Length (long edge) | 131 mm (5.16") | Elecrow spec; caliper photo agrees within about 1 mm |
| Width (short edge) | 80.2 mm (3.158") | Caliper |
| Thickness, screen + PCB | 5.1 mm (0.20") | Caliper |
| Thickness, deepest part (connectors on back, no battery) | 16.5 mm (0.65") | Caliper |
| Visible screen glass, long side | 119 mm (4.7") | Caliper |
| PCB lip around the glass | about 5 mm (0.2") per side | Caliper |
| Mounting holes | 4, one per corner, on the PCB lip outside the glass | Visual |
| Hole diameter | 2.5 mm (0.10"), fits M2.5 screws | Caliper |
| Hole center from edges | about 3.5 mm from both edges | Estimated from photo. Confirm with the test-fit plate |

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

## Open

- Hole-center positions: print the test-fit plate (`hardware/test-fit-plate.scad`) and check the screws line up before printing the full case.
