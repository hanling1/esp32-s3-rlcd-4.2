#include "stock_data.h"
#include "stock_config.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "stock_data";

#define RESP_BUF_SIZE 2048
#define TOKEN_MAX     64

static stock_quote_t s_quotes[STOCK_COUNT];
static SemaphoreHandle_t s_mutex;

static bool parse_record(const char *start, stock_quote_t *out)
{
    const char *tokens[TOKEN_MAX];
    int count = 0;
    const char *p = start;
    tokens[count++] = p;
    while (*p && *p != '"' && count < TOKEN_MAX) {
        if (*p == '~') {
            tokens[count++] = p + 1;
        }
        p++;
    }

    if (count <= 34) {
        return false;
    }

    float price = strtof(tokens[3], NULL);
    if (price <= 0.0f) {
        return false;
    }

    out->price         = price;
    out->prev_close    = strtof(tokens[4], NULL);
    out->open          = strtof(tokens[5], NULL);
    out->change_amount = strtof(tokens[31], NULL);
    out->change_pct    = strtof(tokens[32], NULL);
    out->high          = strtof(tokens[33], NULL);
    out->low           = strtof(tokens[34], NULL);

    const char *ts = tokens[30];
    int n = 0;
    while (ts[n] && ts[n] != '~' && n < 14) {
        out->update_time[n] = ts[n];
        n++;
    }
    out->update_time[n] = '\0';

    out->valid = true;
    return true;
}

/* Parses a batched response with one v_...="..."; line per requested stock,
 * matching each line to its watchlist index by the code embedded in the
 * v_<secid> assignment name. Writes matched quotes into results[]; leaves
 * unmatched/failed entries with valid=false. Returns the number parsed. */
static int parse_batched(const char *body, stock_quote_t *results)
{
    int parsed = 0;
    for (int i = 0; i < STOCK_COUNT; i++) {
        results[i].valid = false;

        char needle[24];
        snprintf(needle, sizeof(needle), "v_%s=", STOCK_WATCHLIST[i].secid);
        const char *line = strstr(body, needle);
        if (!line) {
            continue;
        }
        const char *quote = strchr(line, '"');
        if (!quote) {
            continue;
        }
        if (parse_record(quote + 1, &results[i])) {
            parsed++;
        }
    }
    return parsed;
}

static bool fetch_all(stock_quote_t *results)
{
    char *url = malloc(RESP_BUF_SIZE);
    char *buf = malloc(RESP_BUF_SIZE);
    if (!url || !buf) {
        free(url);
        free(buf);
        return false;
    }

    int pos = snprintf(url, RESP_BUF_SIZE, "%s", STOCK_QUOTE_BASE_URL);
    for (int i = 0; i < STOCK_COUNT; i++) {
        pos += snprintf(url + pos, RESP_BUF_SIZE - pos, "%s%s",
                        (i == 0) ? "" : ",", STOCK_WATCHLIST[i].secid);
    }

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 8000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    bool ok = false;

    if (esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int len = esp_http_client_read(client, buf, RESP_BUF_SIZE - 1);
        if (len > 0) {
            buf[len] = '\0';
            ok = (parse_batched(buf, results) > 0);
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(url);
    free(buf);
    return ok;
}

static void poll_task(void *arg)
{
    stock_quote_t local[STOCK_COUNT];
    for (;;) {
        memset(local, 0, sizeof(local));
        if (fetch_all(local)) {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            for (int i = 0; i < STOCK_COUNT; i++) {
                if (local[i].valid) {
                    s_quotes[i] = local[i];
                }
            }
            xSemaphoreGive(s_mutex);
            ESP_LOGI(TAG, "updated %d stocks, [0] price=%.2f pct=%.2f%%",
                     (int)STOCK_COUNT, local[0].price, local[0].change_pct);
        } else {
            ESP_LOGW(TAG, "fetch/parse failed, keeping previous");
        }
        vTaskDelay(pdMS_TO_TICKS(STOCK_POLL_PERIOD_MS));
    }
}

esp_err_t stock_data_start(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        return ESP_ERR_NO_MEM;
    }
    memset(s_quotes, 0, sizeof(s_quotes));
    if (xTaskCreate(poll_task, "stock_poll", 6144, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void stock_data_get(int idx, stock_quote_t *out)
{
    if (idx < 0 || idx >= STOCK_COUNT) {
        memset(out, 0, sizeof(*out));
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_quotes[idx];
    xSemaphoreGive(s_mutex);
}
