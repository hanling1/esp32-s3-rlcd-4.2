# Tasks

## 1. Watchlist config
- [x] 1.1 In `stock_config.h`, replace the scalar `STOCK_SECID` / `STOCK_CODE` / `STOCK_NAME_UTF8` / `STOCK_QUOTE_URL` macros with a compile-time table of `{ secid, code, name_utf8 }` and a `STOCK_COUNT`; keep the 3 entries `sz002859 洁美科技`, `sz002946 新乳业`, `sz000636 风华高科`
- [x] 1.2 Keep `STOCK_POLL_PERIOD_MS`; define the base endpoint (`http://qt.gtimg.cn/q=`) so the batched URL can be built at runtime

## 2. Multi-stock data layer
- [x] 2.1 In `stock_data.h`, change storage to an array and change the getter to `stock_data_get(int idx, stock_quote_t *out)`; add a count accessor (or expose `STOCK_COUNT`)
- [x] 2.2 In `stock_data.c`, build the batched URL by joining all watchlist secids with commas
- [x] 2.3 `fetch_once`: issue ONE GET for the batched URL into the read buffer
- [x] 2.4 Parse EACH returned `v_...="...";` line; match each line to its watchlist stock by code; write into `s_quotes[idx]` under the mutex
- [x] 2.5 Per-stock fault tolerance: on a line's parse failure keep that stock's previous snapshot; other stocks still update
- [x] 2.6 Update `stock_data_get` to copy the requested index's snapshot under the lock

## 3. Button switching (main loop)
- [x] 3.1 Add `current_idx` state in `main.c`
- [x] 3.2 KEY (gpio18): on release, if held < reset threshold → select previous (`idx = (idx - 1 + N) % N`); the ≥3s reset path is unchanged and calls `esp_restart()` (so no switch is emitted after a reset)
- [x] 3.3 BOOT (gpio0): short press → select next (`idx = (idx + 1) % N`), debounced so one press = one step
- [x] 3.4 Ensure single-press = single step for both buttons (edge/debounce handling)

## 4. UI: selected stock + page indicator
- [x] 4.1 In `stock_ui.c`, render the stock at `current_idx`: name (from the watchlist table), code, price, change %, open/high/low/prev-close, update time
- [x] 4.2 Add a `[i/N]` (1-based) page indicator
- [x] 4.3 Keep all `lv_*` calls inside `bsp_lvgl_lock()` / `bsp_lvgl_unlock()`
- [x] 4.4 Pass/read `current_idx` from main to the UI refresh (signature or shared accessor)

## 5. Font + build + verify
- [x] 5.1 Regenerate `font_stock_16.c` adding glyphs `新 乳 业 风 华` to `lv_font_conv --symbols` (科/高 already present)
- [x] 5.2 `idf.py build` clean on ESP-IDF v6.0.2
- [x] 5.3 Hardware: all 3 stocks poll and show live values; KEY=previous, BOOT=next, wrapping; `[i/N]` correct
- [x] 5.4 Hardware: KEY long-press 3s still resets Wi-Fi (switch not emitted); short press switches
- [x] 5.5 Update README watchlist section (3 stocks, switching keys, regenerated font symbols)
