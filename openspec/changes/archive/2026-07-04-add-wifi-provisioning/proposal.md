## Why

Today Wi-Fi credentials are hardcoded in `stock_config.h` (`STOCK_WIFI_SSID` / `STOCK_WIFI_PASSWORD`) and compiled into the firmware. Changing networks (new home, new router, giving the device to someone else) requires editing source and reflashing. This was explicitly deferred to v2 in the `add-stock-quote` change.

This change adds **runtime Wi-Fi provisioning** so the user can set the network without reflashing: on first boot (or after a reset) the device hosts its own Wi-Fi hotspot with a captive-portal web page; the user connects with a phone/laptop, picks the network and enters the password in a browser, and the device saves it to NVS and connects.

> We evaluated the official `network_provisioning` manager (SoftAP/BLE, needs the Espressif app), SmartConfig (fragile on modern networks), and on-device text entry (painful with only 3 buttons on a no-touch screen). We chose the **DIY SoftAP + captive portal** approach: no phone app required (any browser), lowest flash cost, no BLE stack, and full control — the best fit for a 3-button monochrome appliance.

## What Changes

- **Reuse a proven open-source captive-portal implementation** ([zhy345517-rgb/esp32_wifi_configuration](https://github.com/zhy345517-rgb/esp32_wifi_configuration), studied during exploration) as the starting point, adapted to this project:
  - **Port in as-is**: a `dns_server` component (standard DNS-hijack for the captive portal; this is the same implementation as Espressif's official `captive_portal` example) and the portal web page `index.html` (~8 KB, scan list + password field, already polished).
  - **Port in with changes**: the `wifi_portal` state machine (Wi-Fi scan → serve portal → parse form → save to NVS → connect → auto-close portal on success, with boot-time direct-connect and fallback-to-portal). Remove its NTP integration (we don't need time sync; the quote feed carries its own timestamp).
- **Replace the current `wifi_sta` module** with `wifi_portal`. The portal's "have saved credentials → connect STA directly" path is a superset of what `wifi_sta` does today, so the two must not coexist (both would init `esp_wifi` and register events → conflict).
- **Add a factory-reset gesture** (not in the reference repo): long-press the `KEY` button for ~3 seconds to erase saved credentials and reboot into provisioning. Implemented in application code by polling GPIO level (the BSP already exposes single-press events; long-press is derived in the app, no BSP change needed).
- **Add on-screen provisioning guidance** on the monochrome LCD: when in portal mode, show the hotspot name (`Stock_XXXXXX`) and the portal URL (`192.168.4.1`) so the user knows what to connect to. Requires adding a few fixed Chinese glyphs to the subset font.
- **Config changes** in `stock_config.h`: remove `STOCK_WIFI_SSID` / `STOCK_WIFI_PASSWORD`; add AP name prefix (`Stock`), open (no-password) hotspot, and the reset hold duration.

## Capabilities

### Modified Capabilities
- `wifi-connectivity`: was "connect to a hardcoded SSID". Now: connect to NVS-stored credentials if present; otherwise host a SoftAP captive portal for the user to provide them at runtime; persist on success; support a long-press factory reset. Hardcoded-credentials requirement is removed.
- `stock-ui`: add a provisioning-mode screen state showing hotspot name and portal URL, and a "resetting" indication.

### New Capabilities
<!-- None as separate specs — provisioning is folded into the existing wifi-connectivity capability since it replaces the STA-only behavior. dns_server + index.html are implementation details, not user-facing capabilities. -->

## Impact

- **New code / components**: `components/stock_app/dns_server/` (or as a sibling component), `index.html` embedded in the build, a rewritten Wi-Fi module (`wifi_portal`) replacing `wifi_sta.c`, long-press detection, provisioning UI.
- **Removed**: `wifi_sta.c` / `wifi_sta.h`; `STOCK_WIFI_SSID` / `STOCK_WIFI_PASSWORD` macros; the reference repo's `ntp_time` component (not ported).
- **Dependencies**: adds `esp_http_server` (portal) and `lwip` sockets (DNS); `esp_wifi` / `nvs_flash` / `esp_netif` / `esp_event` already used. No BLE, no managed components.
- **Consumers**: `stock_data.c` and `stock_ui.c` currently call `wifi_sta_*`; they switch to the new module's status API (`wifi_portal_get_state`). `main.c` calls `wifi_portal_start()` instead of `wifi_sta_start()`.
- **Font**: add fixed glyphs for the provisioning screen (e.g. 配网 / 请连接 / 热点 / 浏览器 / 打开 / 重置).
- **Attribution**: the ported code originates from a third-party repo with no LICENSE file; this is for personal/learning use. Source is credited in comments/README. `dns_server` matches Espressif's public example, which is the safer provenance.
- **Verification**: requires hardware — first-boot portal, phone connects and submits, device saves + connects + shows quote; long-press KEY wipes and returns to portal.

## Non-Goals

- Official `network_provisioning` manager / Espressif phone app / BLE provisioning.
- On-device text entry of the password via buttons + on-screen keyboard.
- QR code on the provisioning screen (possible later enhancement).
- Multiple saved networks / network switching UI (single stored network only).
- Encrypted portal (HTTPS) or protocomm security — the portal is plain HTTP on a short-lived local hotspot.
- Changing which stock is tracked at runtime (still fixed to 002859).
