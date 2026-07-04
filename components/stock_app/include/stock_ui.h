#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t stock_ui_create(void);

/* Reads the watchlist[idx] quote + Wi-Fi status and repaints. Takes the LVGL
 * lock internally, so callers must NOT hold it. */
void stock_ui_refresh(int idx);

#ifdef __cplusplus
}
#endif
