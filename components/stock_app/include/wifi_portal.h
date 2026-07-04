/* SoftAP captive-portal Wi-Fi provisioning. Ported from
 * https://github.com/zhy345517-rgb/esp32_wifi_configuration (NTP removed,
 * reset added). Reused for personal/learning use. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_PORTAL_STATUS_IDLE = 0,
    WIFI_PORTAL_STATUS_CONNECTING,
    WIFI_PORTAL_STATUS_CONNECTED,
    WIFI_PORTAL_STATUS_FAILED,
} wifi_portal_connect_status_t;

typedef struct {
    const char *ap_ssid_prefix;
    const char *ap_password;
    uint8_t ap_channel;
    uint8_t max_connection;
} wifi_portal_config_t;

typedef struct {
    wifi_portal_connect_status_t status;
    char ap_ssid[33];
    char sta_ip[16];
    char fail_reason[64];
} wifi_portal_state_t;

#define WIFI_PORTAL_DEFAULT_CONFIG()                   \
    {                                                  \
        .ap_ssid_prefix = "Stock",                     \
        .ap_password = NULL,                           \
        .ap_channel = 1,                               \
        .max_connection = 4,                           \
    }

esp_err_t wifi_portal_start(const wifi_portal_config_t *config);
esp_err_t wifi_portal_stop(void);
void wifi_portal_get_state(wifi_portal_state_t *state);
const char *wifi_portal_status_string(wifi_portal_connect_status_t status);

/* Erases stored credentials from NVS. Caller reboots afterward to re-provision. */
esp_err_t wifi_portal_erase_credentials(void);

#ifdef __cplusplus
}
#endif
