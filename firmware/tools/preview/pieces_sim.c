/* SPDX-License-Identifier: MIT */
/* Desktop piece source: reads the 12 PNGs written by `npm run board:preview-pieces`. */
#include "pieces.h"
#include <stdio.h>

const void *eg_piece_src(char color, char type)
{
    static char paths[2][128][24];
    static char used[128];
    int ci = color == 'w' ? 0 : 1;
    if (!used[(int)type]) {
        for (int c = 0; c < 2; c++)
            snprintf(paths[c][(int)type], sizeof paths[c][0], "A:pieces/%c%c.png", c == 0 ? 'w' : 'b', type);
        used[(int)type] = 1;
    }
    return paths[ci][(int)type];
}

/* Sets-tab previews: the sample pieces stand in for every set. */
const void *eg_thumb_find(const char *url) { return url && url[0] ? eg_piece_src('w', url[1]) : 0; }
const void *eg_thumb_load(const char *url) { return eg_thumb_find(url); }
