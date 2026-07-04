#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "st7305.h"
#include "bsp_rlcd42_pins.h"

static const char *TAG = "st7305";

#define ST7305_SPI_HOST      SPI3_HOST
#define ST7305_PIXELS        (ST7305_H_RES * ST7305_V_RES)
#define ST7305_BUF_LEN       (ST7305_PIXELS / 8)

static esp_lcd_panel_io_handle_t s_io = NULL;
static uint8_t *s_disp_buf = NULL;

/* Landscape LUT (AlgorithmOptimization == 3): precomputed byte index and bit
 * mask for every (x, y). Allocated in SPIRAM as pointer-to-array of V_RES. */
static uint16_t (*s_idx)[ST7305_V_RES] = NULL;
static uint8_t  (*s_bit)[ST7305_V_RES] = NULL;

static void st7305_send_command(uint8_t reg)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_io, reg, NULL, 0));
}

static void st7305_send_data(uint8_t data)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_io, -1, &data, 1));
}

static void st7305_send_buffer(const uint8_t *data, int len)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(s_io, -1, data, len));
}

static void st7305_set_reset_level(uint8_t level)
{
    gpio_set_level((gpio_num_t)BSP_LCD_GPIO_RST, level ? 1 : 0);
}

static void st7305_reset(void)
{
    st7305_set_reset_level(1);
    vTaskDelay(pdMS_TO_TICKS(50));
    st7305_set_reset_level(0);
    vTaskDelay(pdMS_TO_TICKS(20));
    st7305_set_reset_level(1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void st7305_init_landscape_lut(void)
{
    const uint16_t H4 = ST7305_V_RES >> 2;
    for (uint16_t y = 0; y < ST7305_V_RES; y++) {
        uint16_t inv_y = ST7305_V_RES - 1 - y;
        uint16_t block_y = inv_y >> 2;
        uint8_t  local_y = inv_y & 3;
        for (uint16_t x = 0; x < ST7305_H_RES; x++) {
            uint16_t byte_x = x >> 1;
            uint8_t  local_x = x & 1;
            uint32_t index = byte_x * H4 + block_y;
            uint8_t bit = 7 - ((local_y << 1) | local_x);
            s_idx[x][y] = (uint16_t)index;
            s_bit[x][y] = (uint8_t)(1 << bit);
        }
    }
}

void st7305_set_pixel(uint16_t x, uint16_t y, uint8_t color)
{
    if (x >= ST7305_H_RES || y >= ST7305_V_RES) {
        return;
    }
    uint8_t *p = &s_disp_buf[s_idx[x][y]];
    uint8_t mask = s_bit[x][y];
    if (color) {
        *p |= mask;
    } else {
        *p &= ~mask;
    }
}

void st7305_color_clear(uint8_t color)
{
    memset(s_disp_buf, color, ST7305_BUF_LEN);
}

void st7305_display(void)
{
    st7305_send_command(0x2A);      // Column Address Set
    st7305_send_data(0x12);
    st7305_send_data(0x2A);

    st7305_send_command(0x2B);      // Page Address Set
    st7305_send_data(0x00);
    st7305_send_data(0xC7);

    st7305_send_command(0x2C);      // Memory Write

    st7305_send_buffer(s_disp_buf, ST7305_BUF_LEN);
}

static void st7305_run_init_sequence(void)
{
    st7305_reset();

    st7305_send_command(0xD6);
    st7305_send_data(0x17);
    st7305_send_data(0x02);

    st7305_send_command(0xD1);
    st7305_send_data(0x01);

    st7305_send_command(0xC0);
    st7305_send_data(0x11);
    st7305_send_data(0x04);

    st7305_send_command(0xC1);
    st7305_send_data(0x69);
    st7305_send_data(0x69);
    st7305_send_data(0x69);
    st7305_send_data(0x69);

    st7305_send_command(0xC2);
    st7305_send_data(0x19);
    st7305_send_data(0x19);
    st7305_send_data(0x19);
    st7305_send_data(0x19);

    st7305_send_command(0xC4);
    st7305_send_data(0x4B);
    st7305_send_data(0x4B);
    st7305_send_data(0x4B);
    st7305_send_data(0x4B);

    st7305_send_command(0xC5);
    st7305_send_data(0x19);
    st7305_send_data(0x19);
    st7305_send_data(0x19);
    st7305_send_data(0x19);

    st7305_send_command(0xD8);
    st7305_send_data(0x80);
    st7305_send_data(0xE9);

    st7305_send_command(0xB2);
    st7305_send_data(0x02);

    st7305_send_command(0xB3);
    st7305_send_data(0xE5);
    st7305_send_data(0xF6);
    st7305_send_data(0x05);
    st7305_send_data(0x46);
    st7305_send_data(0x77);
    st7305_send_data(0x77);
    st7305_send_data(0x77);
    st7305_send_data(0x77);
    st7305_send_data(0x76);
    st7305_send_data(0x45);

    st7305_send_command(0xB4);
    st7305_send_data(0x05);
    st7305_send_data(0x46);
    st7305_send_data(0x77);
    st7305_send_data(0x77);
    st7305_send_data(0x77);
    st7305_send_data(0x77);
    st7305_send_data(0x76);
    st7305_send_data(0x45);

    st7305_send_command(0x62);
    st7305_send_data(0x32);
    st7305_send_data(0x03);
    st7305_send_data(0x1F);

    st7305_send_command(0xB7);
    st7305_send_data(0x13);

    st7305_send_command(0xB0);
    st7305_send_data(0x64);

    st7305_send_command(0x11);
    vTaskDelay(pdMS_TO_TICKS(200));
    st7305_send_command(0xC9);
    st7305_send_data(0x00);

    st7305_send_command(0x36);
    st7305_send_data(0x48);

    st7305_send_command(0x3A);
    st7305_send_data(0x11);

    st7305_send_command(0xB9);
    st7305_send_data(0x20);

    st7305_send_command(0xB8);
    st7305_send_data(0x29);

    st7305_send_command(0x21);

    st7305_send_command(0x2A);
    st7305_send_data(0x12);
    st7305_send_data(0x2A);

    st7305_send_command(0x2B);
    st7305_send_data(0x00);
    st7305_send_data(0xC7);

    st7305_send_command(0x35);
    st7305_send_data(0x00);

    st7305_send_command(0xD0);
    st7305_send_data(0xFF);

    st7305_send_command(0x38);
    st7305_send_command(0x29);

    st7305_color_clear(ST7305_WHITE);
}

esp_err_t st7305_init(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = BSP_LCD_GPIO_MOSI,
        .sclk_io_num = BSP_LCD_GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7305_PIXELS,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(ST7305_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = BSP_LCD_GPIO_DC,
        .cs_gpio_num = BSP_LCD_GPIO_CS,
        .pclk_hz = 10 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)ST7305_SPI_HOST,
                                             &io_config, &s_io));

    gpio_config_t gpio_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (0x1ULL << BSP_LCD_GPIO_RST),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));
    st7305_set_reset_level(1);

    s_disp_buf = (uint8_t *)heap_caps_malloc(ST7305_BUF_LEN, MALLOC_CAP_SPIRAM);
    assert(s_disp_buf);

    s_idx = heap_caps_malloc(ST7305_PIXELS * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    s_bit = heap_caps_malloc(ST7305_PIXELS * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
    assert(s_idx);
    assert(s_bit);
    st7305_init_landscape_lut();

    st7305_run_init_sequence();
    st7305_display();

    ESP_LOGI(TAG, "ST7305 %dx%d initialized", ST7305_H_RES, ST7305_V_RES);
    return ESP_OK;
}

esp_err_t st7305_flush(int x1, int y1, int x2, int y2, const uint8_t *bitmap)
{
    if (bitmap == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int w = x2 - x1 + 1;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            int bit_index = (y - y1) * w + (x - x1);
            uint8_t byte = bitmap[bit_index >> 3];
            uint8_t set = byte & (0x80 >> (bit_index & 7));
            st7305_set_pixel((uint16_t)x, (uint16_t)y, set ? ST7305_BLACK : ST7305_WHITE);
        }
    }
    st7305_display();
    return ESP_OK;
}

void st7305_draw_test_pattern(void)
{
    st7305_color_clear(ST7305_WHITE);

    for (int x = 0; x < ST7305_H_RES; x++) {
        st7305_set_pixel((uint16_t)x, 0, ST7305_BLACK);
        st7305_set_pixel((uint16_t)x, ST7305_V_RES - 1, ST7305_BLACK);
    }
    for (int y = 0; y < ST7305_V_RES; y++) {
        st7305_set_pixel(0, (uint16_t)y, ST7305_BLACK);
        st7305_set_pixel(ST7305_H_RES - 1, (uint16_t)y, ST7305_BLACK);
    }

    for (int y = 0; y < ST7305_V_RES; y++) {
        for (int x = 0; x < ST7305_H_RES; x++) {
            if (((x >> 4) + (y >> 4)) & 1) {
                st7305_set_pixel((uint16_t)x, (uint16_t)y, ST7305_BLACK);
            }
        }
    }

    st7305_display();
}
