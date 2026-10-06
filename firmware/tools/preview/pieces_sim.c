/* SPDX-License-Identifier: MIT */
/* Desktop piece source: reads the 12 PNGs written by `npm run board:preview-pieces`. */
#include <stdbool.h>
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
const void *eg_thumb_find(const char *url) { return url && url[0] && url[1] ? eg_piece_src('w', url[1]) : 0; }
const void *eg_thumb_load(const char *url) { return eg_thumb_find(url); }
bool eg_thumb_failed(const char *url) { return !url || !url[0] || url[0] == 'x'; }

/* Profile pictures: a gradient disc stands in for any URL. */
#include "lvgl.h"
#include <math.h>
#include <string.h>
static uint32_t av_px[2][96 * 96];
static lv_image_dsc_t av_dsc[2];
const void *eg_avatar_find(const char *url, int size)
{
    if (!url || !url[0] || size > 96) return NULL;
    int k = size > 50;
    if (!av_dsc[k].data) {
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) {
                float dx = x + 0.5f - size / 2.0f, dy = y + 0.5f - size / 2.0f;
                float d = size / 2.0f - sqrtf(dx * dx + dy * dy);
                uint32_t a = d <= 0 ? 0 : d < 1 ? (uint32_t)(255 * d) : 255;
                uint32_t r = 255, g = (uint32_t)(46 + 150 * y / size), b = (uint32_t)(177 + 70 * x / size);
                av_px[k][y * size + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        av_dsc[k].header.magic = LV_IMAGE_HEADER_MAGIC;
        av_dsc[k].header.cf = LV_COLOR_FORMAT_ARGB8888;
        av_dsc[k].header.w = av_dsc[k].header.h = size;
        av_dsc[k].header.stride = size * 4;
        av_dsc[k].data = (const uint8_t *)av_px[k];
        av_dsc[k].data_size = size * size * 4;
    }
    return &av_dsc[k];
}
const void *eg_avatar_load(const char *url, int size) { return eg_avatar_find(url, size); }
