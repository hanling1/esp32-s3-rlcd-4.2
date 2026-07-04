## Context

This is a greenfield initialization for the **Waveshare ESP32-S3-RLCD-4.2** board. The board pairs an ESP32-S3-WROOM-1-N16R8 module (16 MB flash, 8 MB octal PSRAM) with a **4.2" reflective monochrome LCD** driven by an **ST7305 controller over SPI**. There is **no touchscreen** — input is three physical buttons (BOOT, PWR, KEY). The board also carries audio (ES8311/ES7210), sensors (SHTC3, PCF85063 RTC), and an SD slot, but these are out of scope for initial bring-up.

Constraints:
- Official support targets **ESP-IDF v5.x** (recommend v5.3+) and Arduino. No official ESP-BSP component exists for this board.
- Reference material: Waveshare `waveshareteam/ESP32-S3-RLCD-4.2` repo (`02_Example/ESP-IDF`) and the official schematic. Community ESPHome pin mappings cover the display only and must be verified against the schematic.
- The display is 1-bit (black/white), 300×400 native portrait (400×300 landscape in community configs).

## Goals / Non-Goals

**Goals:**
- A buildable, flashable ESP-IDF v5.x project targeting `esp32s3` with correct 16 MB flash + 8 MB octal PSRAM (80 MHz) configuration.
- A reusable BSP component that centralizes all pin mappings and peripheral init in one place.
- ST7305 monochrome SPI display driver wired into LVGL v8.x at 1-bit color depth.
- Button input (BOOT/PWR/KEY) surfaced to the app / LVGL.
- A sample LVGL screen proving the full pipeline (init → render → input → build → flash).

**Non-Goals:**
- Audio codec (ES8311/ES7210), microphone array, and speaker output.
- Sensors (SHTC3 temp/humidity, PCF85063 RTC, IMU) and SD card storage.
- Wi-Fi / BLE connectivity, OTA, or the XiaoZhi AI voice-assistant firmware variant.
- Power/battery management beyond defaults.
- Grayscale/anti-aliased rendering (panel is 2-level B/W).

## Decisions

**1. Framework: ESP-IDF v5.3+ (over Arduino/PlatformIO).**
Rationale: Full control over SPI timing and PSRAM config needed for a non-standard reflective panel; official demos are ESP-IDF-first; better long-term maintainability for a BSP. Alternative (Arduino) is faster to prototype but abstracts away the SPI/PSRAM control we need and bundles vendor libs that are harder to version.

**2. Graphics: LVGL v8.x via `esp_lvgl_port` managed component (over raw ST7305 draws or U8g2).**
Rationale: The requested scope is a "Full BSP + LVGL app". `esp_lvgl_port` provides a maintained tick/timer/task integration and a clean display-driver registration API. LVGL supports `LV_COLOR_DEPTH 1` for monochrome panels. Alternative U8g2 is lighter but not an LVGL-grade UI framework; raw draws don't meet the "LVGL app" goal.

**3. Display driver: custom ST7305 SPI driver in the BSP, referencing the Waveshare ESP-IDF example.**
Rationale: No official `esp_lcd` vendor component ships for ST7305 for this board. We adapt the Waveshare `02_Example/ESP-IDF` init sequence into an `esp_lcd_panel`-style driver and register it with LVGL via a monochrome flush callback (pack pixels into 1-bpp column/page format per ST7305). Alternative (wait for an upstream component) blocks progress.

**4. Component layout: single `components/bsp_rlcd42/` component.**
Rationale: Keeps pin map, display driver, and buttons cohesive and reusable; `main/` stays thin (app + LVGL screen only). Alternative (everything in `main/`) hurts reuse and testability.

**5. Pin mappings live in one header (`bsp_rlcd42_pins.h`), verified against the official schematic.**
Community-derived pins (CLK=11, MOSI=12, CS=40, DC=5, RST=41; KEY/battery on GPIO4/46) are the starting point but MUST be confirmed against `ESP32-S3-RLCD-4.2-schematic.pdf` before the driver is considered done.

## Risks / Trade-offs

- **[Community pin mappings may be incomplete/incorrect]** → Treat the schematic PDF as the source of truth; a task explicitly verifies every GPIO before sign-off; isolate all pins in one header so corrections are one-line changes.
- **[ST7305 lacks a ready-made esp_lcd component]** → Port the init sequence and flush packing from the official Waveshare ESP-IDF example; validate visually on hardware with a test pattern before wiring LVGL.
- **[Monochrome flush format mismatch (page vs. column addressing)]** → Implement and unit-verify the 1-bpp packing against a known test pattern (checkerboard/border) first, then hand the buffer to LVGL.
- **[PSRAM misconfiguration causes boot loops or no framebuffer]** → Pin `sdkconfig.defaults` to octal PSRAM @ 80 MHz and N16R8 flash; document the exact settings.
- **[No hardware on hand during scaffolding]** → Structure so `idf.py build` succeeds without a board; flash-and-verify steps are clearly marked as requiring physical hardware.
- **[LVGL v8 vs v9 API drift]** → Pin LVGL to a specific v8.x version in the component manifest to avoid breaking-change churn.

## Migration Plan

Greenfield — no migration. Rollback = discard the change (delete scaffolded files); nothing pre-existing is modified.

## Open Questions

- Exact ST7305 addressing mode (page vs. column) and refresh command sequence — resolve from the Waveshare ESP-IDF example + datasheet during driver implementation.
- Whether `esp_lvgl_port` supports 1-bpp cleanly or a thin custom LVGL display glue is needed — validate during display bring-up.
- Preferred default screen orientation (portrait 300×400 vs. landscape 400×300) — default to landscape 400×300 to match community configs; confirm with user if it matters.
