## Context

The `add-stock-quote` change shipped with hardcoded Wi-Fi credentials and explicitly deferred provisioning. This change makes credentials runtime-configurable. Hardware constraints are unchanged: 400×300 monochrome ST7305 LCD (LVGL, black/white, no touch), 3 buttons (BOOT/PWR/KEY), ESP-IDF v6.0.2, ESP32-S3 (2.4 GHz only).

During exploration we compared provisioning approaches (official `network_provisioning` SoftAP/BLE, SmartConfig, on-device keyboard, SoftAP captive portal) and chose **DIY SoftAP + captive portal**: no phone app, lowest flash, no BLE, full control. We then found a high-quality reference implementation ([zhy345517-rgb/esp32_wifi_configuration](https://github.com/zhy345517-rgb/esp32_wifi_configuration)) that matches this design and decided to reuse it rather than write from scratch.

## Goals / Non-Goals

**Goals:**
- User sets Wi-Fi at runtime via a browser captive portal, no reflash, no phone app.
- Credentials persist in NVS; on boot, connect directly if present.
- If no credentials or connect fails, fall back to the portal.
- Long-press KEY (~3 s) erases credentials and returns to the portal (factory reset).
- On-screen guidance tells the user the hotspot name and portal URL.

**Non-Goals:**
- Official manager / BLE / SmartConfig / on-device keyboard / QR / HTTPS / multi-network (see proposal).

## Decisions

**1. Reuse the reference repo; adapt, don't fork wholesale.**
- `dns_server` (DNS hijack) — port in as-is. It matches Espressif's official `captive_portal` example (standard, ~340 lines, no changes needed).
- `index.html` (~8 KB portal page: scan list + password field + status polling) — port in as-is, embedded via `EMBED_FILES`.
- `wifi_portal.c` (the state machine) — port in with edits: remove the `ntp_time` calls and dependency; keep scan/portal/form-parse/NVS/APSTA-fallback logic.
Rationale: the logic is already event-driven, defensive, and covers the exact flow we want. Rewriting would add risk for no benefit. Provenance: repo has no LICENSE (personal/learning use), credited in source; `dns_server` = Espressif public sample.

**2. `wifi_portal` replaces `wifi_sta.c` (not coexist).**
Both initialize `esp_wifi`, register event handlers, and set mode. Running both would double-init and conflict. The portal's "saved credentials → connect STA directly" branch is functionally a superset of `wifi_sta_start()`. So `wifi_sta.c`/`.h` are deleted and consumers switch to the portal's status API.

**3. NVS is authoritative; use the portal's own namespace.**
The reference stores SSID/pass in NVS namespace `wifi_portal` (keys `sta_ssid`/`sta_pass`) and sets `WIFI_STORAGE_RAM` so the driver's own persistence doesn't fight it. We keep this: our namespace is the single source of truth. "Not provisioned" = namespace/key absent (`ESP_ERR_NVS_NOT_FOUND`). Reset = erase this namespace.

**4. Long-press reset is application-level, no BSP change.**
`bsp_button_get_pressed_event` only reports single presses, but `gpio_get_level` is readable. The app's main loop tracks how long KEY is held; ≥3 s → erase NVS namespace → `esp_restart()`. Keeps the BSP untouched (respects its "verified base" status).

```
main loop (1 Hz UI tick, plus faster button sampling):
  if KEY level == pressed:
      hold_ms += dt
      if hold_ms >= 3000: show "重置中" → erase creds → esp_restart()
  else:
      hold_ms = 0
```

**5. UI reads Wi-Fi state, doesn't own it.**
`stock_ui_refresh()` already reads a status enum. It will read `wifi_portal_get_state()` and render one of:
- portal mode → "配网模式 / 请连接热点 Stock_XXXXXX / 浏览器打开 192.168.4.1"
- connecting → "连接中"
- connected → the quote screen (existing)
- resetting → "重置中"
The hotspot SSID comes from `wifi_portal_state_t.ap_ssid` (built from MAC suffix), so the on-screen name always matches the real AP.

**6. AP config defaults (confirmed with user).**
- SSID prefix `Stock` → runtime name `Stock_XXXXXX` (last 3 MAC bytes; avoids collisions with multiple units).
- Open hotspot (no password) — short-lived, convenience over security.
- Channel 1, small `max_connection`.

## Risks / Trade-offs

- **[IDF 5.5 → 6.0.2 API drift]** The reference targets IDF 5.5.x. It uses only core `esp_wifi`/`esp_http_server`/`nvs`/`lwip` APIs (not the removed `wifi_provisioning`), so it should compile on 6.0.2 → verify with a clean build early.
- **[Two Wi-Fi modules during migration]** Mitigated by deleting `wifi_sta.c` in the same change and switching all consumers atomically.
- **[Plain-HTTP portal]** Credentials cross an open local hotspot in cleartext. Acceptable: hotspot is local, short-lived, single-purpose appliance (documented non-goal).
- **[Accidental reset]** 3 s hold on KEY is deliberate to avoid stray taps; show "重置中" as confirmation feedback before wiping.
- **[Portal socket limits]** Reference already caps `max_open_sockets = 5` for LWIP limits — keep it.
- **[Font size creep]** A handful of new CJK glyphs (~10) add ~1 KB to the subset font. Negligible.

## Migration Plan

Additive + replacement on branch `stock` (or a feature branch). Steps: port `dns_server` + `index.html`; port+edit `wifi_portal.c`; delete `wifi_sta.*`; switch `main.c` / `stock_data.c` / `stock_ui.c` to the new API; add long-press reset; add provisioning UI + font glyphs; remove SSID/PASS macros. Rollback = restore `wifi_sta.c` and revert consumers (the `add-stock-quote` v1.0.0 tag is a known-good fallback).

## Open Questions

- Exact button sampling cadence for reliable long-press (main loop is 1 Hz for UI; may need a faster poll or a short sub-loop while KEY is held).
- Whether to keep `wifi_portal`'s boot-time retry count (3) or tune it before falling back to the portal.
- Portal auto-close vs. keeping AP up briefly after success (reference closes immediately on got-IP; fine for us).
- Component layout: `dns_server` as its own component vs. nested under `stock_app` — confirm during implementation.
