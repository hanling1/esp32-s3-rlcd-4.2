#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_lvgl_init(void);

void bsp_lvgl_lock(void);

void bsp_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif
