# Tasks

## 1. Wi-Fi connectivity (STA, hardcoded)
- [x] 1.1 Initialize `nvs_flash` (erase + re-init if partition invalid) before Wi-Fi start
- [x] 1.2 Bring up Wi-Fi STA with hardcoded SSID / password, using event handlers for connect/disconnect/got-IP. NOTE: ESP32-S3 is 2.4 GHz-only; the working SSID is `solaso_2.4G` (the `solaso_5G` 5 GHz band is invisible to the radio, reason=201 NO_AP_FOUND).
- [x] 1.3 Implement auto-reconnect on disconnect without blocking the UI task
- [x] 1.4 Expose a connection status flag/enum the UI can read (connecting / connected / failed)
- [x] 1.5 Verify on hardware: device connects and obtains an IP; logs show got-IP event

## 2. Stock data client
- [x] 2.1 Pin exact `~` field indices against a live `curl 'http://qt.gtimg.cn/q=sz002859'` sample. Confirmed (0-index of `~`-split tokens after opening `"`): [1]=name [2]=code [3]=price [4]=prev-close [5]=open [30]=timestamp(YYYYMMDDhhmmss) [31]=change amount [32]=change % [33]=high [34]=low
- [x] 2.2 Implement `esp_http_client` GET with a small response buffer and sane timeout
- [x] 2.3 Implement defensive `~`-split parser into a typed `quote_t`; bounds-checked, tolerates short/garbled records; validates price is a positive number before accepting
- [x] 2.4 Create a 5 s poll loop (FreeRTOS task) that fetches → parses → updates shared `quote_t` (single-writer or mutex-guarded) and marks it dirty
- [x] 2.5 Fault tolerance: on timeout/connection/parse failure, keep looping, do not crash, do not overwrite good values with garbage
- [x] 2.6 Verify on hardware: serial logs show parsed fields matching the live web value during trading hours

## 3. Chinese font subset
- [x] 3.1 Enumerate all fixed glyphs the UI shows (洁美科技 + fixed labels + ↑↓ + digits/punct) into a symbol list
- [x] 3.2 Generate a 1-bpp subset font `.c` with `lv_font_conv --bpp 1 --symbols ...` (14–16 px)
- [x] 3.3 Add the generated `.c` to the build and declare with `LV_FONT_DECLARE`; do NOT modify the managed LVGL component
- [x] 3.4 Verify on hardware: 洁美科技 renders correctly (no missing/box glyphs)

## 4. Stock UI screen
- [x] 4.1 Build the LVGL layout: name (洁美科技) + code (002859) + update time; large current price; change % with ↑/↓; open/high/low/prev-close rows
- [x] 4.2 Apply the subset font via `lv_style_set_text_font`; design pure black-on-white for the binarized panel
- [x] 4.3 Implement a UI refresh that reads the shared `quote_t` and updates labels inside `bsp_lvgl_lock()`/`bsp_lvgl_unlock()`
- [x] 4.4 Implement the no-data / connecting state ("连接中" or `--` placeholders) shown before first data and on failure
- [x] 4.5 Verify on hardware: screen shows live values and updates every ~5 s during trading hours

## 5. App integration
- [x] 5.1 Rewrite `main.c` flow: `bsp_init` → `bsp_lvgl_init` → build UI → Wi-Fi up → start poll task
- [x] 5.2 Decide code location (default: `components/stock_app/`); wire `CMakeLists.txt` REQUIRES (`bsp_rlcd42`, `lvgl`, `esp_wifi`, `esp_http_client`, `nvs_flash`)
- [x] 5.3 Confirm `sdkconfig` Wi-Fi enabled and UTF-8 text handling; no PSRAM/target/color-depth changes
- [x] 5.4 `idf.py build` clean; run `lsp`/compile diagnostics

## 6. End-to-end verification
- [x] 6.1 Flash and monitor on hardware; confirm connect → fetch → render pipeline works
- [x] 6.2 Verify fault tolerance: disconnect Wi-Fi mid-run → UI shows status, no crash; reconnect → resumes
- [x] 6.3 Confirm out-of-hours behavior is understood (static close price; timestamp visible)
- [x] 6.4 Update `README.md` with the new app usage and the deferred-provisioning note
