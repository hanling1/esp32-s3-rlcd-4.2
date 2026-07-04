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

/* Starts a background task that polls every watchlist stock every 5 s in one
 * batched request and keeps a per-stock snapshot up to date. Call
 * wifi_portal_start() first. */
esp_err_t stock_data_start(void);

/* Copies the latest snapshot for watchlist index idx into out. quote.valid is
 * false until that stock's first successful fetch and stays true afterwards
 * even across transient failures. Out-of-range idx yields an invalid quote. */
void stock_data_get(int idx, stock_quote_t *out);

#ifdef __cplusplus
}
#endif
