#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STA_STATUS_CONNECTING = 0,
    WIFI_STA_STATUS_CONNECTED,
    WIFI_STA_STATUS_FAILED,
} wifi_sta_status_t;

/* Initializes NVS (required by the Wi-Fi stack) then starts STA mode.
 * Returns after start; connecting and auto-reconnect run in the background. */
esp_err_t wifi_sta_start(void);

wifi_sta_status_t wifi_sta_get_status(void);

bool wifi_sta_is_connected(void);

#ifdef __cplusplus
}
#endif
