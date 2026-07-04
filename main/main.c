#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_rlcd42.h"
#include "bsp_lvgl.h"
#include "wifi_sta.h"
#include "stock_data.h"
#include "stock_ui.h"

static const char *TAG = "app";

void app_main(void)
{
    ESP_LOGI(TAG, "stock quote viewer booting...");

    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(bsp_lvgl_init());
    ESP_ERROR_CHECK(stock_ui_create());

    ESP_ERROR_CHECK(wifi_sta_start());
    ESP_ERROR_CHECK(stock_data_start());

    for (;;) {
        stock_ui_refresh();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
