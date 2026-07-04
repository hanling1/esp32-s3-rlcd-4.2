#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BSP_BTN_BOOT = 0,
    BSP_BTN_PWR,
    BSP_BTN_KEY,
    BSP_BTN_COUNT,
} bsp_button_t;

esp_err_t bsp_buttons_init(void);

bool bsp_button_get_pressed_event(bsp_button_t button);

#ifdef __cplusplus
}
#endif
