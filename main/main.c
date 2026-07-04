#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "bsp_rlcd42.h"
#include "bsp_lvgl.h"
#include "lvgl.h"

static const char *TAG = "app";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3-RLCD-4.2 booting...");

    ESP_ERROR_CHECK(bsp_init());
    ESP_ERROR_CHECK(bsp_lvgl_init());

    bsp_lvgl_lock();
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Hello ESP32-S3-RLCD-4.2");
    lv_obj_center(label);
    bsp_lvgl_unlock();
}
