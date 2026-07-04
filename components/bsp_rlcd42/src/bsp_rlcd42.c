#include "bsp_rlcd42.h"
#include "st7305.h"
#include "bsp_buttons.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "bsp_rlcd42";

esp_err_t bsp_init(void)
{
    ESP_RETURN_ON_ERROR(st7305_init(), TAG, "st7305");
    ESP_RETURN_ON_ERROR(bsp_buttons_init(), TAG, "buttons");
    return ESP_OK;
}
