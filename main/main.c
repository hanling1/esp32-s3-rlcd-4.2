#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "bsp_rlcd42.h"
#include "bsp_rlcd42_pins.h"
#include "bsp_lvgl.h"
#include "wifi_portal.h"
#include "stock_config.h"
#include "stock_data.h"
#include "stock_ui.h"

static const char *TAG = "app";

void app_main(void)
{
    ESP_LOGI(TAG, "stock quote viewer booting...");

    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(bsp_lvgl_init());
    ESP_ERROR_CHECK(stock_ui_create());

    wifi_portal_config_t portal_cfg = WIFI_PORTAL_DEFAULT_CONFIG();
    portal_cfg.ap_ssid_prefix = STOCK_AP_SSID_PREFIX;
    ESP_ERROR_CHECK(wifi_portal_start(&portal_cfg));
    ESP_ERROR_CHECK(stock_data_start());

    int64_t key_down_us = 0;
    bool right_was_down = false;
    int current_idx = 0;
    int ui_tick = 0;

    for (;;) {
        if (++ui_tick >= 10) {
            ui_tick = 0;
            stock_ui_refresh(current_idx);
        }

        /* LEFT key (gpio18) is active-low with pull-up. Holding it for
         * STOCK_RESET_HOLD_MS clears stored credentials and reboots into the
         * provisioning portal; a shorter press selects the previous stock. */
        if (gpio_get_level(BSP_BTN_GPIO_LEFT) == 0) {
            if (key_down_us == 0) {
                key_down_us = esp_timer_get_time();
            } else if (esp_timer_get_time() - key_down_us >= STOCK_RESET_HOLD_MS * 1000LL) {
                ESP_LOGW(TAG, "LEFT key held %d ms, erasing credentials and rebooting", STOCK_RESET_HOLD_MS);
                wifi_portal_erase_credentials();
                esp_restart();
            }
        } else {
            if (key_down_us != 0) {
                current_idx = (current_idx + STOCK_COUNT - 1) % STOCK_COUNT;
                stock_ui_refresh(current_idx);
            }
            key_down_us = 0;
        }

        /* RIGHT key (gpio0/BOOT) is active-low; a press selects the next stock.
         * Edge-detected on release so a single tap advances exactly once. */
        bool right_down = (gpio_get_level(BSP_BTN_GPIO_RIGHT) == 0);
        if (!right_down && right_was_down) {
            current_idx = (current_idx + 1) % STOCK_COUNT;
            stock_ui_refresh(current_idx);
        }
        right_was_down = right_down;

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
