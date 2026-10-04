/* SPDX-License-Identifier: MIT */
#include "pieces_store.h"
#include "api.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "pieces.h"
#include <string.h>

static const char *TAG = "eg_pieces";
static lv_image_dsc_t dsc[12];     /* color*6 + type; RAW PNG data, decoded by LVGL's lodepng decoder */
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

bool eg_pieces_load(const char *token, bool *changed)
{
    char set_id[48] = {0};
    static char urls[12][160];
    *changed = false;
    if (eg_api_pieces(token, set_id, urls) != EG_OK) return dsc[0].data != NULL;
    if (strcmp(set_id, current_set) == 0 && dsc[0].data) return true;

    void *fresh[12] = {0};
    size_t len[12] = {0};
    for (int i = 0; i < 12; i++) {
        fresh[i] = eg_http_download(urls[i], &len[i]);
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
        dsc[i].header.cf = LV_COLOR_FORMAT_RAW;
        dsc[i].data = data[i];
        dsc[i].data_size = len[i];
    }
    snprintf(current_set, sizeof current_set, "%s", set_id);
    *changed = true;
    return true;
}
