#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool valid;
    float price;
    float prev_close;
    float open;
    float high;
    float low;
    float change_amount;
    float change_pct;
    char update_time[16];
} stock_quote_t;

/* Starts a background task that polls the quote endpoint every 5 s and keeps
 * the shared snapshot up to date. Call wifi_portal_start() first. */
esp_err_t stock_data_start(void);

/* Copies the latest snapshot into out. quote.valid is false until the first
 * successful fetch and stays true afterwards even across transient failures. */
void stock_data_get(stock_quote_t *out);

#ifdef __cplusplus
}
#endif
