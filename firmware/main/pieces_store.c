/* SPDX-License-Identifier: MIT */
#include "pieces_store.h"
#include "api.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "pieces.h"
#include <string.h>

static const char *TAG = "eg_pieces";
static lv_image_dsc_t dsc[12];     /* color*6 + type; ARGB8888 at EG_PIECE_PX, decoded once at download */
static void *data[12];
static char current_set[48];

static int type_index(char t)
{
    const char *order = "kqrbnp";
    const char *p = strchr(order, t);
    return p ? (int)(p - order) : -1;
}

const void *eg_piece_src(char color, char type)
{
    int t = type_index(type);
    if (t < 0) return NULL;
    int i = (color == 'b' ? 6 : 0) + t;
    return dsc[i].data ? &dsc[i] : NULL;
}

#include "libs/lodepng/lodepng.h"
#include <stdlib.h>

/* PNG -> ARGB8888 (LVGL byte order B,G,R,A) resized to EG_PIECE_PX with an alpha-weighted box filter.
 * Decoding once here keeps every board redraw a plain copy. Returns PSRAM memory or NULL. */
static uint8_t *decode_scaled_locked(const void *png, size_t len)
{
    unsigned char *rgba = NULL;
    unsigned w = 0, h = 0;
    /* lodepng allocates through LVGL (lv_malloc), so its buffer goes back with lv_free, never free() */
    unsigned err = lodepng_decode32(&rgba, &w, &h, png, len);
    if (err || !rgba || !w || !h) {
        ESP_LOGW(TAG, "png decode: %s", err ? lodepng_error_text(err) : "empty image");
        lv_free(rgba);
        return NULL;
    }
    const int N = EG_PIECE_PX;
    uint8_t *out = heap_caps_malloc(N * N * 4, MALLOC_CAP_SPIRAM);
    if (!out) { lv_free(rgba); return NULL; }
    for (int y = 0; y < N; y++) {
        unsigned y0 = y * h / N, y1 = (y + 1) * h / N; if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < N; x++) {
            unsigned x0 = x * w / N, x1 = (x + 1) * w / N; if (x1 <= x0) x1 = x0 + 1;
            uint32_t r = 0, g = 0, b = 0, a = 0, n = 0;
            for (unsigned sy = y0; sy < y1 && sy < h; sy++)
                for (unsigned sx = x0; sx < x1 && sx < w; sx++) {
                    const unsigned char *p = rgba + (sy * w + sx) * 4;
                    r += p[0] * p[3]; g += p[1] * p[3]; b += p[2] * p[3]; a += p[3]; n++;
                }
            uint8_t *o = out + (y * N + x) * 4;
            if (a) { o[0] = b / a; o[1] = g / a; o[2] = r / a; } else { o[0] = o[1] = o[2] = 0; }
            o[3] = n ? a / n : 0;
        }
    }
    lv_free(rgba);
    return out;
}

/* PNG -> ARGB8888 (LVGL byte order B,G,R,A) resized to EG_PIECE_PX with an alpha-weighted box filter.
 * Runs on the network task: LVGL's allocator isn't thread-safe, so hold the LVGL lock while lodepng uses it.
 * Returns PSRAM memory or NULL. */
static uint8_t *decode_scaled(const void *png, size_t len)
{
    lvgl_port_lock(0);
    uint8_t *out = decode_scaled_locked(png, len);
    lvgl_port_unlock();
    return out;
}

bool eg_pieces_load(const char *token, bool *changed)
{
    char set_id[48] = {0};
    static char urls[12][160];
    *changed = false;
    if (eg_api_pieces(token, set_id, urls) != EG_OK) return dsc[0].data != NULL;
    if (strcmp(set_id, current_set) == 0 && dsc[0].data) return true;

    void *fresh[12] = {0};
    for (int i = 0; i < 12; i++) {
        for (int attempt = 0; attempt < 3 && !fresh[i]; attempt++) {
            size_t len = 0;
            void *png = eg_http_download(urls[i], &len);
            fresh[i] = png ? decode_scaled(png, len) : NULL;
            if (!fresh[i]) ESP_LOGW(TAG, "piece %d attempt %d: %s (%u bytes), internal free %u, largest %u", i, attempt + 1,
                                    png ? "decode failed" : "download failed", (unsigned)len,
                                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
            heap_caps_free(png);
        }
        if (!fresh[i]) {
            ESP_LOGW(TAG, "piece %d failed", i);
            for (int j = 0; j < i; j++) heap_caps_free(fresh[j]);
            return dsc[0].data != NULL;     /* keep the old set */
        }
    }
    /* Swap in. The UI task reads dsc[] too, so the caller should hold the LVGL lock around this function's result use. */
    for (int i = 0; i < 12; i++) {
        heap_caps_free(data[i]);
        data[i] = fresh[i];
        memset(&dsc[i], 0, sizeof dsc[i]);
        dsc[i].header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc[i].header.cf = LV_COLOR_FORMAT_ARGB8888;
        dsc[i].header.w = EG_PIECE_PX;
        dsc[i].header.h = EG_PIECE_PX;
        dsc[i].header.stride = EG_PIECE_PX * 4;
        dsc[i].data = data[i];
        dsc[i].data_size = EG_PIECE_PX * EG_PIECE_PX * 4;
    }
    snprintf(current_set, sizeof current_set, "%s", set_id);
    *changed = true;
    return true;
}

/* ---------- set preview thumbnails (Sets tab) ---------- */

#define THUMB_MAX 40
static struct { char url[160]; lv_image_dsc_t dsc; } thumbs[THUMB_MAX];
static int thumbs_n;

const void *eg_thumb_find(const char *url)
{
    if (!url || !url[0]) return NULL;
    for (int i = 0; i < thumbs_n; i++) if (strcmp(thumbs[i].url, url) == 0) return &thumbs[i].dsc;
    return NULL;
}

const void *eg_thumb_load(const char *url)
{
    const void *have = eg_thumb_find(url);
    if (have || !url || !url[0] || thumbs_n >= THUMB_MAX) return have;
    size_t len = 0;
    void *png = eg_http_download(url, &len);
    uint8_t *px = png ? decode_scaled(png, len) : NULL;
    heap_caps_free(png);
    if (!px) return NULL;
    lv_image_dsc_t *d = &thumbs[thumbs_n].dsc;
    memset(d, 0, sizeof *d);
    d->header.magic = LV_IMAGE_HEADER_MAGIC;
    d->header.cf = LV_COLOR_FORMAT_ARGB8888;
    d->header.w = d->header.h = EG_PIECE_PX;
    d->header.stride = EG_PIECE_PX * 4;
    d->data = px;
    d->data_size = EG_PIECE_PX * EG_PIECE_PX * 4;
    snprintf(thumbs[thumbs_n].url, sizeof thumbs[0].url, "%s", url);
    return &thumbs[thumbs_n++].dsc;
}
