## Context

This change adds the first real application on top of the verified ESP32-S3-RLCD-4.2 base (Wi-Fi + HTTP + LVGL on a 400×300 1-bit monochrome ST7305 panel, ESP-IDF v6.0.2, LVGL v8.3.11). The goal is a single-stock realtime quote viewer for A-share `002859` (洁美科技).

Constraints:
- No touchscreen; 3 physical buttons only (unused in v1).
- Monochrome panel: no color; up/down conveyed with ↑/↓ glyphs, not red/green.
- Memory-constrained MCU: prefer plain text parsing over JSON libraries; keep large buffers in PSRAM.
- LVGL is non-thread-safe: all `lv_*` calls must be wrapped in `bsp_lvgl_lock()`/`bsp_lvgl_unlock()`.

Key verified fact: `http://qt.gtimg.cn/q=sz002859` returns realtime data over **plain HTTP** (no TLS, no key) as a single `~`-delimited record, confirmed by direct `curl` during exploration.

## Goals / Non-Goals

**Goals:**
- Connect to Wi-Fi (`solaso_5G`) with hardcoded credentials and auto-reconnect.
- Poll the Tencent endpoint every 5 seconds and parse the quote fields.
- Render a clean numeric quote screen with the hardcoded name 洁美科技 and code 002859.
- Tolerate network failures gracefully (show `--` / keep last, never crash).

**Non-Goals:**
- Wi-Fi provisioning (deferred to a later change).
- Multiple stocks, intraday charts, order book, GBK→UTF-8 conversion.

## Decisions

**1. Data source: Tencent `qt.gtimg.cn` over plain HTTP (not East Money / not akshare).**
Rationale: Direct testing showed Tencent returns a simple `~`-delimited text record over plain HTTP — no TLS (saves RAM and cert handling on the MCU), no API key, and no JSON library needed (just split on `~`). East Money (what akshare uses) returns JSON over HTTPS, which is heavier. akshare itself is Python and cannot run on the device.

**2. Wi-Fi credentials hardcoded in v1; provisioning deferred.**
Rationale: The user chose to postpone provisioning to focus v1 on the data→display main line. Hardcoding `solaso_5G` / password removes an entire subsystem (SoftAP + captive portal + NVS credential store + QR code + button reset) from v1 scope. `nvs_flash_init()` is still required because the Wi-Fi stack uses NVS internally.

**3. Chinese name hardcoded as a UTF-8 constant; no GBK conversion.**
Rationale: The endpoint returns the name in GBK. Since the app tracks exactly one fixed stock, we hardcode `"洁美科技"` as a UTF-8 string literal and ignore the GBK name field entirely. This eliminates the GBK→UTF-8 conversion subsystem (no iconv, no lookup table).

**4. Font: a 1-bpp subset generated with `lv_font_conv`, glyphs limited to the fixed UI text.**
Rationale: The panel is binarized to black/white, so 1-bpp fonts are both smallest and visually correct (higher bpp anti-aliasing is discarded on flush). Because all displayed Chinese is compile-time-fixed (洁美科技 + fixed labels), we embed only those ~30-50 glyphs (~2 KB flash). Declared via `LV_FONT_DECLARE`, added to the build as a `.c` file; the managed LVGL component is not modified.

**5. Parsing: split the `~`-delimited record by field index.**
Rationale: The Tencent record is positional. Observed mapping (index from the `v_sz002859="..."` payload split on `~`): [3]=current price, [4]=prev close, [5]=open, [31]=change amount, [32]=change %, [33]=high, [34]=low, plus an update timestamp field. Exact indices will be pinned during implementation against a live sample; parsing must be defensive (bounds-checked, tolerate short/garbled records).

**6. Architecture: a polling task feeding a shared quote struct; UI updates under the LVGL lock.**
Rationale: A dedicated FreeRTOS task connects Wi-Fi, then loops every 5 s: HTTP GET → parse → update a shared `quote_t` (mutex-guarded or single-writer) → mark dirty. The UI refresh takes `bsp_lvgl_lock()`, writes label text, and unlocks. This matches the base's threading model.

## Risks / Trade-offs

- **[Unofficial endpoint may change format or rate-limit]** → 5 s polling is gentle; parsing is defensive; on failure the UI shows `--`/last value. If Tencent changes fields, only the index mapping needs updating (isolated in the parser).
- **[Field index drift between symbols/markets]** → Pin indices against a live `curl` sample for `sz002859` during implementation; add a parse self-check (e.g. price must be a positive number) before trusting a record.
- **[Out-of-hours data is static]** → Outside trading hours the endpoint returns the last close; 5 s refresh shows no change. This is data behavior, not a bug; the timestamp field makes it visible.
- **[Wi-Fi down / weak signal]** → STA auto-reconnect with a visible "connecting/failed" status; the poll loop retries and never blocks the UI task.
- **[GBK bytes in the raw response]** → We never decode the GBK name field; we only read numeric ASCII fields (safe) and use the hardcoded UTF-8 name. The raw buffer is treated as bytes, split on the ASCII `~` (0x7E), which is unambiguous in GBK.

## Migration Plan

Additive on branch `stock`. No existing specs modified. Rollback = discard the new component/files; the base is untouched. Wi-Fi provisioning is a planned follow-up change that will replace the hardcoded credentials with NVS-stored, user-provisioned ones.

## Open Questions

- Exact Tencent field indices for `sz002859` (price/open/high/low/change%/timestamp) — resolve against a live sample during implementation.
- Whether to place the code in a new `components/stock_app/` component or directly under `main/` — default to a component for reuse/testability; confirm during implementation.
- Font point size (14 vs 16 px) for best legibility on 400×300 — default 16 px for the price, smaller for secondary fields; tune on hardware.
