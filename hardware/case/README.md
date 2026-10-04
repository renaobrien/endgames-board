# Case v0: flat brick

Two printed parts and four screws. Outer size 135.8 x 85.0 x 22.2 mm.

| File | What | Print |
|---|---|---|
| `case-tray.stl` | Back shell. Holds the board on 4 corner posts. Openings for both USB-C ports, the power switch, microSD, and BOOT/RESET holes in the floor | Floor down, no supports |
| `test-fit-tray.stl` | The tray with most of the floor cut out. Same walls, posts and openings. Print this first to check the USB-C ports, power switch, microSD and screw posts | Floor down, no supports. About 40% less plastic |
| `case-lid.stl` | Front frame. Window matches the lit screen area, so no black border shows | Face down, no supports |

Hardware: 4 x M2.5 x 20 mm screws, from the back. They pass through the tray posts and the board's corner holes and bite into the lid's posts.

Source: `brick.py` (Python, trimesh). Change the numbers at the top and re-run to regenerate the STLs.

v0 limits:
- Port and button positions are estimated from a photo of the board with a ruler (about +/- 2 mm). Openings are oversized to allow for that.
- Hole positions are estimated too. Print `../test-fit-plate.stl` first.
- No battery bay yet.
