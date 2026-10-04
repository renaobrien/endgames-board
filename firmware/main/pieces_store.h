/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>

/* Downloads the 12 piece PNGs from board-pieces into PSRAM. Skips the download if set_id is unchanged.
 * Returns true if pieces are available afterwards. Call from the network task, not the UI task. */
bool eg_pieces_load(const char *token, bool *changed);
