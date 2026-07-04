## Why

We need a working firmware foundation for the **Waveshare ESP32-S3-RLCD-4.2** board so future features (UI screens, sensors, audio) can be built on a verified toolchain and hardware bring-up. Starting from an empty repo means every feature would otherwise re-solve board bring-up; a scaffolded BSP + LVGL app removes that friction now.

> Note: "RLCD" = **Reflective LCD** (rectangular monochrome ST7305 over SPI), **not** a round color LCD. The board has **no touchscreen** — input is via physical BOOT / PWR / KEY buttons.

## What Changes

- Add an **ESP-IDF v5.x project scaffold** (CMake, `sdkconfig.defaults`, `main` component) targeting `esp32s3` with 16 MB flash and 8 MB octal PSRAM enabled.
- Add a **board support layer (BSP)** for the ESP32-S3-RLCD-4.2 that centralizes pin mappings and initializes core peripherals.
- Add an **ST7305 monochrome SPI display driver** integration and wire it to **LVGL v8.x** (1-bit color depth, monochrome-friendly flush).
- Add **physical button input** handling (BOOT / PWR / KEY) as the primary input method (no touch driver).
- Add a **sample LVGL application screen** ("Hello" / status screen) to prove display + input + build pipeline end-to-end.
- Add build/flash documentation and `.gitignore` for ESP-IDF build artifacts.

## Capabilities

### New Capabilities
- `board-bsp`: Board support package for ESP32-S3-RLCD-4.2 — centralized pin map, clock/PSRAM config, and peripheral bring-up entry point.
- `display-driver`: ST7305 monochrome reflective LCD driver over SPI integrated with LVGL as the rendering backend.
- `input-buttons`: Physical button (BOOT/PWR/KEY) input handling exposed to the application/LVGL.
- `sample-ui-app`: Minimal LVGL sample application that renders a status screen and responds to button input.

### Modified Capabilities
<!-- None — this is a greenfield initialization; no existing specs. -->

## Impact

- **New code**: `CMakeLists.txt`, `sdkconfig.defaults`, `main/` (app entry + LVGL init), `components/bsp_rlcd42/` (BSP + ST7305 driver + buttons), LVGL managed component dependency.
- **Dependencies**: ESP-IDF v5.3+, LVGL (`lvgl/lvgl` or `espressif/esp_lvgl_port` managed component), ESP-IDF SPI/GPIO drivers. Reference: Waveshare `02_Example/ESP-IDF` demo and official schematic.
- **Hardware**: Requires the physical ESP32-S3-RLCD-4.2 board for flash-and-verify; PSRAM must be octal @ 80 MHz.
- **Systems**: No backend/API impact — self-contained embedded firmware repo.
