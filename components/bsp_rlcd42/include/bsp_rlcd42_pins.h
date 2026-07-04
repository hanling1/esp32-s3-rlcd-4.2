#pragma once

/*
 * Central GPIO pin map for the Waveshare ESP32-S3-RLCD-4.2 board.
 * Source of truth: official ESP32-S3-RLCD-4.2 schematic (verified in task 2.3).
 * All GPIO usage across the BSP MUST reference these constants.
 */

#define BSP_LCD_GPIO_SCLK   11
#define BSP_LCD_GPIO_MOSI   12
#define BSP_LCD_GPIO_CS     40
#define BSP_LCD_GPIO_DC     5
#define BSP_LCD_GPIO_RST    41

#define BSP_BTN_GPIO_BOOT   0
#define BSP_BTN_GPIO_PWR    46
#define BSP_BTN_GPIO_KEY    4
