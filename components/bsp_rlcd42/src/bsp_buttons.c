#include "bsp_buttons.h"
#include "bsp_rlcd42_pins.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define BSP_BTN_DEBOUNCE_US 20000

static const gpio_num_t s_gpio[BSP_BTN_COUNT] = {
    BSP_BTN_GPIO_BOOT,
    BSP_BTN_GPIO_PWR,
    BSP_BTN_GPIO_KEY,
};

static int s_last_level[BSP_BTN_COUNT];
static int64_t s_last_us[BSP_BTN_COUNT];

esp_err_t bsp_buttons_init(void)
{
    for (int i = 0; i < BSP_BTN_COUNT; i++) {
        gpio_config_t cfg = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (0x1ULL << s_gpio[i]),
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };
        esp_err_t err = gpio_config(&cfg);
        if (err != ESP_OK) {
            return err;
        }
        s_last_level[i] = 1;
        s_last_us[i] = 0;
    }
    return ESP_OK;
}

bool bsp_button_get_pressed_event(bsp_button_t button)
{
    if (button < 0 || button >= BSP_BTN_COUNT) {
        return false;
    }

    int lvl = gpio_get_level(s_gpio[button]);
    int64_t now = esp_timer_get_time();

    if (now - s_last_us[button] < BSP_BTN_DEBOUNCE_US) {
        return false;
    }

    bool pressed = (s_last_level[button] == 1 && lvl == 0);
    if (s_last_level[button] != lvl) {
        s_last_us[button] = now;
        s_last_level[button] = lvl;
    }
    return pressed;
}
