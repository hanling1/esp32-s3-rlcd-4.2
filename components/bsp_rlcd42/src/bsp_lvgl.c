#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "lvgl.h"

#include "bsp_lvgl.h"
#include "st7305.h"

static const char *TAG = "bsp_lvgl";

#define LVGL_TICK_PERIOD_MS    5
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 5

static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static SemaphoreHandle_t lvgl_mux = NULL;

void bsp_lvgl_lock(void)
{
    xSemaphoreTake(lvgl_mux, portMAX_DELAY);
}

void bsp_lvgl_unlock(void)
{
    xSemaphoreGive(lvgl_mux);
}

static bool lock_ms(int ms)
{
    const TickType_t ticks = (ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(ms);
    return xSemaphoreTake(lvgl_mux, ticks) == pdTRUE;
}

static void increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    uint16_t *b = (uint16_t *)color_map;
    for (int y = area->y1; y <= area->y2; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t c = (*b < 0x7fff) ? ST7305_BLACK : ST7305_WHITE;
            st7305_set_pixel((uint16_t)x, (uint16_t)y, c);
            b++;
        }
    }
    st7305_display();
    lv_disp_flush_ready(drv);
}

static void lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    for (;;) {
        if (lock_ms(-1)) {
            task_delay_ms = lv_timer_handler();
            bsp_lvgl_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

esp_err_t bsp_lvgl_init(void)
{
    lvgl_mux = xSemaphoreCreateMutex();
    lv_init();

    size_t px = ST7305_H_RES * ST7305_V_RES;
    lv_color_t *buf1 = heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_color_t *buf2 = heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1);
    assert(buf2);
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, px);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = ST7305_H_RES;
    disp_drv.ver_res = ST7305_V_RES;
    disp_drv.full_refresh = 1;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = flush_cb;
    lv_disp_drv_register(&disp_drv);

    const esp_timer_create_args_t tick_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreatePinnedToCore(lvgl_port_task, "LVGL", 8 * 1024, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "LVGL port initialized");
    return ESP_OK;
}
