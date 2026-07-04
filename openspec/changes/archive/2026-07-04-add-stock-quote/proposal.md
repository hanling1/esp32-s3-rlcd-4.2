## Why

We want to prove the ESP32-S3-RLCD-4.2 base (Wi-Fi + HTTP + LVGL rendering) with a real, useful application: a **single-stock realtime quote viewer**. It fetches live A-share data over Wi-Fi and renders it on the reflective monochrome LCD, exercising the whole networking-to-display pipeline that future creative apps will reuse.

> The user originally referenced the Python `akshare` library. akshare cannot run on an ESP32 — it is only a wrapper around upstream HTTP JSON APIs. We instead call the same class of public endpoint directly from the device. Direct testing confirmed **Tencent's `qt.gtimg.cn`** endpoint returns realtime data for the target stock over **plain HTTP** (no TLS, no API key), as a simple `~`-delimited text record — ideal for a memory-constrained MCU.

## What Changes

- Add **Wi-Fi station (STA) connectivity** with **hardcoded credentials** (SSID `solaso_5G`) as the first version. Wi-Fi provisioning (SoftAP + captive portal, NVS storage, QR code, button reset) is explicitly **deferred to a later change**.
- Add a **stock data client** that fetches `http://qt.gtimg.cn/q=sz002859` every **5 seconds**, parses the `~`-delimited response, and extracts price / prev-close / open / high / low / change%.
- Add a **stock quote UI screen** (LVGL) showing the fixed Chinese name **"洁美科技"**, the code `002859`, current price, change % (with ↑/↓ symbol since the panel is monochrome), open/high/low/prev-close, and last-update time.
- Add a **small 1-bpp Chinese font subset** (only the fixed glyphs the UI ever shows) generated via `lv_font_conv`, declared with `LV_FONT_DECLARE`. No GBK→UTF-8 conversion is needed because the stock name is hardcoded as a UTF-8 constant.
- Add **fetch fault tolerance**: on request timeout/failure, show `--` or retain the previous values and keep the app alive (no crash, no reboot loop).

## Capabilities

### New Capabilities
- `wifi-connectivity`: Wi-Fi STA connection using hardcoded credentials, with auto-reconnect and a visible connection status.
- `stock-data`: HTTP client that polls the Tencent realtime endpoint on a fixed interval and parses the `~`-delimited quote record into typed fields.
- `stock-ui`: LVGL screen rendering the single-stock quote (Chinese name, code, price, change, OHLC, timestamp) on the monochrome panel, with a fault-tolerant "no data" state.

### Modified Capabilities
<!-- None — builds on top of the verified base (board-bsp, display-driver) without changing their specs. -->

## Impact

- **New code**: a Wi-Fi + stock component (e.g. `components/stock_app/` or additions under `main/`), a generated font C file, and a rewritten `main.c` app flow (connect Wi-Fi → poll → render).
- **Dependencies**: ESP-IDF `esp_wifi`, `esp_http_client`, `nvs_flash` (Wi-Fi needs NVS init), plus the existing `bsp_rlcd42` base and LVGL. No new managed components required for v1.
- **Config**: `sdkconfig` Wi-Fi already enabled; ensure UTF-8 text encoding in LVGL. No PSRAM/target/color-depth changes.
- **Hardware**: Requires the physical board on Wi-Fi range of `solaso_5G` to verify live data end-to-end.
- **Deferred**: Wi-Fi provisioning, multi-stock lists, intraday charts, and dynamic (GBK) stock names are out of scope for this change.

## Non-Goals

- Wi-Fi provisioning / configuration UI (hardcoded credentials only in v1).
- More than one stock (fixed to `002859` / 洁美科技).
- Intraday line charts or historical data (numeric list only).
- Dynamic Chinese names requiring GBK→UTF-8 conversion (name is hardcoded).
- Buy/sell order book, financial ratios beyond the basic OHLC + change fields.
