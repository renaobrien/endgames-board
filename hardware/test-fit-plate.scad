// Endgames Board: test-fit plate for the CrowPanel Advanced 5" (ESP32-P4).
// Print it, lay the board on it face up, and check all 4 holes line up.
// Edit the numbers below if they don't, then the case uses the corrected values.
L = 131;       // board length, mm
W = 80.2;      // board width, mm (caliper)
T = 2;         // plate thickness
HOLE_D = 2.7;  // clearance for M2.5
HOLE_IN = 3.5; // hole center from each edge (estimated from photo)
BORDER = 9;    // frame width
NOTCH = 6;     // notch marks the USB-C edge

difference() {
  cube([L, W, T]);
  translate([BORDER, BORDER, -1]) cube([L - 2*BORDER, W - 2*BORDER, T + 2]);
  for (x = [HOLE_IN, L - HOLE_IN], y = [HOLE_IN, W - HOLE_IN])
    translate([x, y, -1]) cylinder(d = HOLE_D, h = T + 2, $fn = 40);
  translate([L - NOTCH/2, W/2 - NOTCH/2, -1]) cube([NOTCH, NOTCH, T + 2]);
}
