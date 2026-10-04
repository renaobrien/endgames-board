/* SPDX-License-Identifier: MIT */
/* Board support for the Elecrow CrowPanel Advanced 5" ESP32-P4 (SKU DHE04005D, PCB V1.0).
 * The only hardware-specific file. Everything else (UI, API, storage) is board independent.
 *
 * Hardware facts (from Elecrow's published schematics and sample firmware for this board):
 *   Power      LDO channel 3 = 2.5 V, LDO channel 4 = 3.3 V (feed the panel and peripherals)
 *   I2C        port 0, SDA GPIO45, SCL GPIO46, internal pull-ups. Shared by touch and the backlight MCU
 *   Backlight  STC8H1K MCU at I2C 0x2F. Register 0x20 = backlight PWM duty, 0..100
 *   Touch      GT911 on the shared I2C bus, RST GPIO36, INT GPIO42
 *   Display    16-bit RGB565 parallel, 800x480, PCLK 25 MHz on GPIO3, DE GPIO2, HSYNC GPIO40, VSYNC GPIO41
 *              data D0..D15 = GPIO 8,7,6,5,4,14,13,12,11,10,9,19,18,17,16,15
 *              timing: HSYNC 4, HBP 8, HFP 8, VSYNC 4, VBP 16, VFP 16; data latched on PCLK falling edge
 *   Wi-Fi      onboard ESP32-C6 over SDIO slot 1 (4-bit): CMD 54, CLK 53, D0 52, D1 51, D2 50, D3 49,
 *              C6 reset GPIO20 (active high). Configured in sdkconfig.defaults; the standard esp_wifi API
 *              is provided by esp_wifi_remote + esp_hosted.
 */
#include "bsp.h"

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lvgl_port.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"

static const char *TAG = "eg_bsp";

#define EG_H_RES 800
#define EG_V_RES 480

#define EG_I2C_SDA 45
#define EG_I2C_SCL 46
#define EG_TOUCH_RST 36
#define EG_TOUCH_INT 42

#define EG_BL_MCU_ADDR 0x2F
#define EG_BL_REG_PWM 0x20

static esp_ldo_channel_handle_t s_ldo3, s_ldo4;
static i2c_master_bus_handle_t s_i2c;
static i2c_master_dev_handle_t s_bl_dev;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_touch_handle_t s_touch;
static lv_display_t *s_disp;

static esp_err_t power_init(void)
{
    esp_ldo_channel_config_t ldo3 = {.chan_id = 3, .voltage_mv = 2500};
    esp_ldo_channel_config_t ldo4 = {.chan_id = 4, .voltage_mv = 3300};
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo3, &s_ldo3), TAG, "ldo3");
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo4, &s_ldo4), TAG, "ldo4");
    return ESP_OK;
}

static esp_err_t i2c_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = 0,
        .sda_io_num = EG_I2C_SDA,
        .scl_io_num = EG_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&cfg, &s_i2c), TAG, "i2c bus");
    i2c_device_config_t bl = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = EG_BL_MCU_ADDR, .scl_speed_hz = 100000};
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c, &bl, &s_bl_dev), TAG, "backlight mcu");
    return ESP_OK;
}

void eg_bsp_backlight(uint8_t percent)
{
    if (!s_bl_dev) return;
    if (percent > 100) percent = 100;
    uint8_t buf[2] = {EG_BL_REG_PWM, percent};
    esp_err_t e = i2c_master_transmit(s_bl_dev, buf, sizeof(buf), 100);
    if (e != ESP_OK) ESP_LOGW(TAG, "backlight write failed: %s", esp_err_to_name(e));
}

static esp_err_t touch_init(void)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c, &io_cfg, &io), TAG, "touch io");
    esp_lcd_touch_config_t cfg = {
        .x_max = EG_H_RES,
        .y_max = EG_V_RES,
        .rst_gpio_num = EG_TOUCH_RST,
        .int_gpio_num = EG_TOUCH_INT,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
    };
    return esp_lcd_touch_new_i2c_gt911(io, &cfg, &s_touch);
}

static esp_err_t panel_init(void)
{
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 2,
        .bounce_buffer_size_px = 20 * EG_H_RES,
        .dma_burst_size = 64,
        .disp_gpio_num = -1,
        .pclk_gpio_num = 3,
        .vsync_gpio_num = 41,
        .hsync_gpio_num = 40,
        .de_gpio_num = 2,
        .data_gpio_nums = {8, 7, 6, 5, 4, 14, 13, 12, 11, 10, 9, 19, 18, 17, 16, 15},
        .timings = {
            .pclk_hz = 25 * 1000 * 1000,
            .h_res = EG_H_RES,
            .v_res = EG_V_RES,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 16,
            .vsync_front_porch = 16,
            .flags = {.pclk_active_neg = true, .pclk_idle_high = true},
        },
        .flags.fb_in_psram = true,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &s_panel), TAG, "rgb panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
    return ESP_OK;
}

lv_display_t *eg_bsp_init(void)
{
    if (power_init() != ESP_OK || i2c_init() != ESP_OK) return NULL;
    eg_bsp_backlight(0);                                  /* dark until the first frame is ready */
    if (panel_init() != ESP_OK) return NULL;
    if (touch_init() != ESP_OK) ESP_LOGE(TAG, "touch init failed; continuing without touch");

    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 16 * 1024;
    if (lvgl_port_init(&port_cfg) != ESP_OK) return NULL;

    lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = s_panel,
        .buffer_size = EG_H_RES * EG_V_RES,
        .double_buffer = false,
        .hres = EG_H_RES,
        .vres = EG_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {.buff_spiram = true, .swap_bytes = false, .full_refresh = false, .direct_mode = true},
    };
    lvgl_port_display_rgb_cfg_t rgb_cfg = {.flags = {.bb_mode = true, .avoid_tearing = true}};
#if CONFIG_EG_ROTATE_180
    /* Upside-down mount (power switch bottom-right). Direct mode can't rotate, so draw into
       partial buffers and let the port rotate each chunk (PPA on the P4) before it hits the panel. */
    disp_cfg.buffer_size = EG_H_RES * 80;
    disp_cfg.double_buffer = true;
    disp_cfg.flags.direct_mode = false;
    disp_cfg.flags.sw_rotate = true;
    rgb_cfg.flags.avoid_tearing = false;
#endif
    s_disp = lvgl_port_add_disp_rgb(&disp_cfg, &rgb_cfg);
    if (!s_disp) { ESP_LOGE(TAG, "lvgl display add failed"); return NULL; }
#if CONFIG_EG_ROTATE_180
    lvgl_port_lock(0);
    lv_display_set_rotation(s_disp, LV_DISPLAY_ROTATION_180);   /* LVGL rotates touch points to match */
    lvgl_port_unlock();
#endif

    if (s_touch) {
        lvgl_port_touch_cfg_t tcfg = {.disp = s_disp, .handle = s_touch};
        if (!lvgl_port_add_touch(&tcfg)) ESP_LOGE(TAG, "lvgl touch add failed");
    }
    eg_bsp_backlight(100);
    return s_disp;
}

/* ---------------- Wi-Fi (through the C6, via esp_wifi_remote) ---------------- */
#define WIFI_GOT_IP BIT0
#define WIFI_FAILED BIT1
static EventGroupHandle_t s_wifi_ev;
static int s_retries;

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retries++ < 5) esp_wifi_connect();
        else xEventGroupSetBits(s_wifi_ev, WIFI_FAILED);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retries = 0;
        xEventGroupSetBits(s_wifi_ev, WIFI_GOT_IP);
    }
}

bool eg_bsp_wifi_connect(const char *ssid, const char *pass, int timeout_ms)
{
    static bool started = false;
    if (!started) {
        s_wifi_ev = xEventGroupCreate();
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
        if (esp_wifi_init(&init) != ESP_OK) { ESP_LOGE(TAG, "wifi init failed (is the C6 running ESP-Hosted?)"); return false; }
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL, NULL);
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL, NULL);
        esp_wifi_set_mode(WIFI_MODE_STA);
        started = true;
    } else {
        esp_wifi_stop();
    }
    xEventGroupClearBits(s_wifi_ev, WIFI_GOT_IP | WIFI_FAILED);
    s_retries = 0;

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, pass ? pass : "", sizeof(wc.sta.password) - 1);
    wc.sta.threshold.authmode = (pass && pass[0]) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    if (esp_wifi_start() != ESP_OK) return false;

    EventBits_t bits = xEventGroupWaitBits(s_wifi_ev, WIFI_GOT_IP | WIFI_FAILED, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    bool ok = bits & WIFI_GOT_IP;
    ESP_LOGI(TAG, "wifi %s", ok ? "connected" : "failed");
    return ok;
}
