/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>

/* Firmware updates over Wi-Fi.
 * The board reads EG_OTA_MANIFEST ({"version":"x.y.z","url":"https://..."}), and if the version is newer
 * than its own, downloads the app image into the idle slot and restarts into it.
 * A new version stays on probation until eg_ota_mark_good(); if it crashes or never gets online first,
 * the bootloader rolls back to the previous version. */
#define EG_OTA_MANIFEST "https://endgam.es/firmware/board.json"

void eg_ota_mark_good(void);                 /* call once Wi-Fi and the server both work */
const char *eg_ota_version(void);
/* Returns false if no update or it failed. On success it restarts and does not return.
 * on_start runs just before the download begins (show an "Updating" screen). */
bool eg_ota_check_and_update(void (*on_start)(const char *new_version));
