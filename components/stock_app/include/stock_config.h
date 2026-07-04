#pragma once

#define STOCK_AP_SSID_PREFIX "Stock"
#define STOCK_RESET_HOLD_MS  3000

typedef struct {
    const char *secid;
    const char *code;
    const char *name_utf8;
} stock_entry_t;

#define STOCK_QUOTE_BASE_URL "http://qt.gtimg.cn/q="
#define STOCK_POLL_PERIOD_MS 5000

static const stock_entry_t STOCK_WATCHLIST[] = {
    { "sz002859", "002859", "洁美科技" },
    { "sz002946", "002946", "新乳业" },
    { "sz000636", "000636", "风华高科" },
};

#define STOCK_COUNT (sizeof(STOCK_WATCHLIST) / sizeof(STOCK_WATCHLIST[0]))
