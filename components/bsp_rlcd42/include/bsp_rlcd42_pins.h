#pragma once

/*
 * Central GPIO pin map for the Waveshare ESP32-S3-RLCD-4.2 board.
 * All GPIO usage across the BSP MUST reference these constants.
 *
 * Button pins verified by on-device GPIO scan (2026-07): the physical left key
 * is gpio18, the right key is gpio0 (BOOT strap). The middle key is not a
 * readable GPIO input. The earlier gpio46/gpio4 values were incorrect.
 */

#define BSP_LCD_GPIO_SCLK   11
#define BSP_LCD_GPIO_MOSI   12
#define BSP_LCD_GPIO_CS     40
#define BSP_LCD_GPIO_DC     5
#define BSP_LCD_GPIO_RST    41

#define BSP_BTN_GPIO_LEFT   18
#define BSP_BTN_GPIO_RIGHT  0
