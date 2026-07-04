/* Captive-portal DNS hijack. Ported from
 * https://github.com/zhy345517-rgb/esp32_wifi_configuration (matches the
 * Espressif esp-idf captive_portal example). Reused for personal/learning use. */
#pragma once

#include "esp_netif_ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DNS_SERVER_MAX_ITEMS
#define DNS_SERVER_MAX_ITEMS 1
#endif

#define DNS_SERVER_CONFIG_SINGLE(queried_name, netif_key) \
    {                                                      \
        .num_of_entries = 1,                              \
        .item = {{.name = queried_name, .if_key = netif_key}} \
    }

typedef struct dns_entry_pair {
    const char *name;
    const char *if_key;
    esp_ip4_addr_t ip;
} dns_entry_pair_t;

typedef struct dns_server_config {
    int num_of_entries;
    dns_entry_pair_t item[DNS_SERVER_MAX_ITEMS];
} dns_server_config_t;

typedef struct dns_server_handle *dns_server_handle_t;

dns_server_handle_t dns_server_start(const dns_server_config_t *config);
void dns_server_stop(dns_server_handle_t handle);

#ifdef __cplusplus
}
#endif
