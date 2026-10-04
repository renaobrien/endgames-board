# SPDX-License-Identifier: MIT
file(GLOB FONTS fonts/*.c)
idf_component_register(
    SRCS "main.c" "api.c" "store.c" "pieces_store.c" "bsp.c" "ui_game.c" "ui_screens.c" "chess_pos.c" ${FONTS}
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_http_client esp_event esp_netif json mbedtls esp-tls
)
