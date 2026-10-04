/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>

/* Downloads the 12 piece PNGs from board-pieces into PSRAM. Skips the download if set_id is unchanged.
 * Returns true if pieces are available afterwards. Call from the network task, not the UI task. */
bool eg_pieces_load(const char *token, bool *changed);

/* Set preview images for the Sets tab, decoded at EG_PIECE_PX and cached by URL.
 * eg_thumb_load downloads (network task only); eg_thumb_find never does (safe from the UI task). */
const void *eg_thumb_load(const char *url);
const void *eg_thumb_find(const char *url);
