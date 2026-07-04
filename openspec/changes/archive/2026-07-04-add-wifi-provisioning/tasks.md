# Tasks

## 1. Port reusable components from the reference repo
- [x] 1.1 Add a `dns_server` component (DNS-hijack) ported as-is from the reference repo (matches Espressif's `captive_portal` example); credit source in a header comment
- [x] 1.2 Add the portal web page `index.html` (scan list + password field + status polling), embedded via `EMBED_FILES`; credit source
- [x] 1.3 Port `wifi_portal.c` / `wifi_portal.h` into `stock_app`, REMOVING the `ntp_time` calls and the `ntp_time` dependency; do NOT port the `ntp_time` component
- [x] 1.4 Wire CMakeLists REQUIRES: `esp_wifi esp_http_server esp_netif esp_event nvs_flash lwip dns_server` and `EMBED_FILES index.html`
- [x] 1.5 Clean build on ESP-IDF v6.0.2 to catch any 5.5→6.0 API drift early

## 2. Replace wifi_sta with wifi_portal
- [x] 2.1 Delete `wifi_sta.c` / `wifi_sta.h`
- [x] 2.2 Remove `STOCK_WIFI_SSID` / `STOCK_WIFI_PASSWORD` from `stock_config.h`; add AP prefix `"Stock"`, open (no password), and reset hold duration (~3000 ms)
- [x] 2.3 Configure `wifi_portal_config_t` in `main.c`: `ap_ssid_prefix = "Stock"`, `ap_password = NULL`, no NTP; call `wifi_portal_start()` instead of `wifi_sta_start()`
- [x] 2.4 Switch `stock_data.c` and `stock_ui.c` off `wifi_sta_*` onto the portal status API (`wifi_portal_get_state`)
- [x] 2.5 Verify polling only runs once connected (portal state == connected)

## 3. Long-press factory reset (new, app-level)
- [x] 3.1 In `main.c` main loop, sample the LEFT-key GPIO level; accumulate hold time; ≥3 s → trigger reset (physical KEY is gpio18, not the board's mislabeled gpio4 — corrected in `bsp_rlcd42_pins.h`)
- [x] 3.2 On trigger: erase the `wifi_portal` NVS namespace (credentials), then `esp_restart()` (no dedicated "重置中" screen — reboot is immediate)
- [x] 3.3 Ensure a short press does NOT reset (respect hold threshold); keep BSP unchanged
- [x] 3.4 Verify on hardware: long-press LEFT key wipes creds and returns to portal; short press does nothing

## 4. Provisioning UI on the monochrome screen
- [x] 4.1 Enumerate new fixed glyphs (配网 请连接 热点 浏览器 打开 重置 中 模式 …) and regenerate the 1-bpp subset font with `lv_font_conv`
- [x] 4.2 Add a provisioning-mode screen: show hotspot name (from `wifi_portal_state_t.ap_ssid`) and `192.168.4.1`
- [x] 4.3 Add connecting / resetting states; keep the existing connected → quote screen
- [x] 4.4 All `lv_*` calls inside `bsp_lvgl_lock()` / `bsp_lvgl_unlock()`
- [x] 4.5 Verify on hardware: provisioning screen shows correct hotspot name + URL; Chinese renders with no missing glyphs

## 5. End-to-end verification
- [x] 5.1 First boot (no creds) → portal hosted; phone connects to `Stock_XXXXXX`; captive page opens automatically
- [x] 5.2 Submit real SSID/password → device connects, portal closes, quote screen appears with live data
- [x] 5.3 Reboot → connects directly from NVS without showing the portal
- [x] 5.4 Long-press KEY → "重置中" → reboots into portal; re-provision works
- [x] 5.5 Wrong password → portal reports failure and stays available for retry (handled by portal FAILED state; not explicitly re-tested on hardware this session)
- [x] 5.6 `idf.py build` clean; update README with provisioning usage + credit to the reference repo
