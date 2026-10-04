/* SPDX-License-Identifier: MIT */
#pragma once
/* Source for a piece image, usable with lv_image_set_src().
 * color: 'w' or 'b'. type: one of k q r b n p (lowercase). Returns NULL if not loaded.
 * Two implementations: pieces_store.c (board, downloads from board-pieces) and
 * tools/preview/pieces_sim.c (desktop, reads PNG files). */
const void *eg_piece_src(char color, char type);

/* Piece images are decoded once and stored at board-square size, so the board draws them 1:1. */
#define EG_PIECE_PX 56
