## 1. Project Scaffold & Toolchain

- [x] 1.1 Create ESP-IDF project structure: root `CMakeLists.txt`, `main/CMakeLists.txt`, `main/main.c`
- [x] 1.2 Add `sdkconfig.defaults` setting target `esp32s3`, 16 MB flash, octal PSRAM @ 80 MHz (N16R8)
- [x] 1.3 Add `.gitignore` for ESP-IDF build artifacts (`build/`, `sdkconfig`, `managed_components/`, `dependencies.lock`)
- [x] 1.4 Run `idf.py set-target esp32s3 && idf.py build` and confirm exit code 0 (blank app boots)

## 2. Board Support Package (BSP) Foundation

- [x] 2.1 Create `components/bsp_rlcd42/` with `CMakeLists.txt` and public include dir
- [x] 2.2 Create `bsp_rlcd42_pins.h` with named GPIO constants (display CLK/MOSI/CS/DC/RESET; BOOT/PWR/KEY)
- [ ] 2.3 Verify every GPIO constant against the official `ESP32-S3-RLCD-4.2-schematic.pdf` and correct as needed
- [x] 2.4 Create `bsp_rlcd42.h`/`.c` exposing a single `bsp_init()` entry point (display + buttons)

## 3. ST7305 Display Driver

- [x] 3.1 Add SPI bus + device initialization for the ST7305 in the BSP
- [x] 3.2 Port the ST7305 reset/init command sequence from the Waveshare `02_Example/ESP-IDF` demo
- [x] 3.3 Implement 1-bpp pixel packing matching the ST7305 2x2-block addressing (landscape LUT, from official example)
- [ ] 3.4 Write a standalone test-pattern function (border + checkerboard) and verify on hardware

## 4. LVGL Integration

- [x] 4.1 Add LVGL v8.x (and `esp_lvgl_port` if used) as managed component dependencies, pinned to a specific version
- [x] 4.2 Configure LVGL color depth: **RGB565 (`LV_COLOR_DEPTH 16`)** + LUT flush to ST7305 (official approach; `LV_COLOR_DEPTH 1` incompatible with the panel's 2x2-block layout)
- [x] 4.3 Register the ST7305 driver as an LVGL display with a `full_refresh` flush callback
- [x] 4.4 Set up LVGL tick + timer task; render a simple label and confirm it appears on the panel

## 5. Button Input

- [x] 5.1 Configure GPIOs for BOOT/PWR/KEY with pull-ups and debouncing in the BSP
- [x] 5.2 Expose a button event API (poll or callback) to the application layer
- [ ] 5.3 Verify a single physical press yields exactly one logical event (no bounce duplicates)

## 6. Sample LVGL Application

- [x] 6.1 In `main.c`, call `bsp_init()`, start LVGL, and render the sample status screen (title/label)
- [ ] 6.2 Wire KEY button to a visible UI change (e.g., increment a counter / toggle a label)
- [x] 6.3 Confirm the sample screen appears on boot on hardware

## 7. Verification & Documentation

- [x] 7.1 Run `idf.py build` and confirm exit code 0 without a board connected
- [x] 7.2 Flash with `idf.py -p <port> flash monitor`; confirm boot, screen render, and no reboot loop
- [ ] 7.3 Confirm KEY press updates the UI on hardware (end-to-end input verification)
- [x] 7.4 Add a `README.md` with build/flash instructions, board summary, and pin-map reference
