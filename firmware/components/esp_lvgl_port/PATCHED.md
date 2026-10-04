# esp_lvgl_port 2.6.3, patched for Endgames

Upstream 2.6.3 only enables RGB panels on ESP32-S3 (`lvgl_port_add_disp_rgb` fails on the P4 with
"RGB is supported only on ESP32S3"). Elecrow ships the same kind of patch in their CrowPanel P4 factory code.

Change: the S3-only `#if` guards for the RGB header, the RGB vsync/bounce callback and
`lvgl_port_add_disp_rgb` also accept `CONFIG_IDF_TARGET_ESP32P4` (src/lvgl9/esp_lvgl_port_disp.c).
The avoid_tearing frame-buffer path is NOT patched (it calls the MIPI-DSI API on P4), so keep avoid_tearing off.
License: Apache-2.0 (upstream), see license.txt.
