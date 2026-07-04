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

#define RESP_BUF_SIZE 1024
#define TOKEN_MAX     64

static stock_quote_t s_quote;
static SemaphoreHandle_t s_mutex;

static bool parse_response(const char *body, stock_quote_t *out)
{
    const char *start = strchr(body, '"');
    if (!start) {
        return false;
    }
    start++;

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

static bool fetch_once(stock_quote_t *out)
{
    char *buf = malloc(RESP_BUF_SIZE);
    if (!buf) {
        return false;
    }

    esp_http_client_config_t config = {
        .url = STOCK_QUOTE_URL,
        .timeout_ms = 8000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    bool ok = false;

    if (esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int len = esp_http_client_read(client, buf, RESP_BUF_SIZE - 1);
        if (len > 0) {
            buf[len] = '\0';
            ok = parse_response(buf, out);
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buf);
    return ok;
}

static void poll_task(void *arg)
{
    stock_quote_t local;
    for (;;) {
        memset(&local, 0, sizeof(local));
        if (fetch_once(&local)) {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            s_quote = local;
            xSemaphoreGive(s_mutex);
            ESP_LOGI(TAG, "price=%.2f pct=%.2f%% time=%s",
                     local.price, local.change_pct, local.update_time);
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
    memset(&s_quote, 0, sizeof(s_quote));
    if (xTaskCreate(poll_task, "stock_poll", 6144, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void stock_data_get(stock_quote_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_quote;
    xSemaphoreGive(s_mutex);
}
