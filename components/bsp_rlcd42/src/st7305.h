#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ST7305_H_RES 400
#define ST7305_V_RES 300

typedef enum {
    ST7305_BLACK = 0,
    ST7305_WHITE = 0xff,
} st7305_color_t;

esp_err_t st7305_init(void);

esp_err_t st7305_flush(int x1, int y1, int x2, int y2, const uint8_t *bitmap);

void st7305_draw_test_pattern(void);

/* Direct framebuffer access used by the LVGL flush callback. */
void st7305_set_pixel(uint16_t x, uint16_t y, uint8_t color);

void st7305_display(void);

void st7305_color_clear(uint8_t color);

#ifdef __cplusplus
}
#endif
