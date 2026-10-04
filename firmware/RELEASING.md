# Releasing board firmware

Boards check https://endgam.es/firmware/board.json at startup and every 6 hours between games.
If the version there is newer than theirs, they download the update, restart into it, and keep it
once it gets back online. If the new version crashes or can't get online, the board goes back to
the previous version on its next restart.

1. Bump the version
   - In `firmware/CMakeLists.txt`, raise `PROJECT_VER` (for example 0.8.0 to 0.9.0).
2. Build
   - `idf.py build`
3. Publish the update file
   - Copy `build/endgames_board.bin` to the Endgames site as `public/firmware/endgames-board-<version>.bin`.
4. Point boards at it
   - Update `public/firmware/board.json`:
     `{"version": "<version>", "url": "https://endgam.es/firmware/endgames-board-<version>.bin"}`
5. Deploy the site
   - Boards pick it up within 6 hours, or right away after a restart.

The USB flasher file (full image at 0x0) is made with
`python -m esptool --chip esp32p4 merge_bin -o endgames-board-v<version>.bin "@flash_args"` from `build/`.
Only needed for first installs; everything after that updates over Wi-Fi.
