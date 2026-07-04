## Context

The stock app currently tracks exactly one hardcoded stock. `stock_config.h` defines scalar macros (`STOCK_SECID`, `STOCK_CODE`, `STOCK_NAME_UTF8`, `STOCK_QUOTE_URL`); `stock_data.c` polls one URL every 5s into a single mutex-guarded `stock_quote_t s_quote` and exposes `stock_data_get(stock_quote_t *out)`; `stock_ui.c` renders the name from the compile-time macro and reads the single quote. Buttons: KEY = gpio18 (left), BOOT = gpio0 (right), both active-low; KEY long-press 3s already erases Wi-Fi credentials in `main.c`'s loop. The subset font (`font_stock_16.c`) embeds only ~40 fixed glyphs.

Two facts from prior investigation shape this design:
- Tencent's `qt.gtimg.cn/q=` endpoint accepts comma-separated secids and returns one `v_...="...";` line per stock, so N stocks cost ONE HTTP request.
- Runtime-arbitrary Chinese names can't render on the monochrome 1-bpp subset font. Since the watchlist is hardcoded, the required glyphs are known at build time and added to the subset.

## Goals / Non-Goals

**Goals:**
- Hardcoded watchlist of 3 stocks, polled together in one batched request.
- On-device switching between stocks via KEY (previous) and BOOT (next), wrapping.
- Page indicator `[i/N]` on screen.
- Preserve the existing KEY long-press 3s Wi-Fi reset (short-press = switch, long-press = reset).
- Keep the subset-font design (add only the new name glyphs).

**Non-Goals:**
- No web/NVS management of the list (deferred; this is the hardcoded precursor).
- No persistent HTTP server on STA (out of scope).
- No arbitrary/runtime stock names or full Chinese font.
- No change to the Wi-Fi provisioning flow beyond sharing the KEY button.

## Decisions

**Watchlist representation.** Replace the scalar macros with a compile-time table of `{ secid, code, name_utf8 }`, plus a count. `stock_config.h` stays the single source of truth. Batched URL is built at runtime by joining the secids with commas.

**Quote storage by index.** `stock_quote_t` gains no name/code (those stay in the config table, indexed in parallel). Storage becomes `s_quotes[STOCK_COUNT]`, still mutex-guarded. Getter becomes `stock_data_get(int idx, stock_quote_t *out)`. A count accessor (`stock_count()` or a public macro) lets the UI/main bound the index.

**Batched fetch + multi-line parse.** `fetch_once` requests the joined URL once, then the parser is applied per returned line. Line-to-index mapping: parse in the order returned (Tencent preserves request order) OR match by code token to be robust; design picks matching-by-code to avoid ordering assumptions. On per-line parse failure, keep that stock's previous snapshot (existing fault-tolerance, per-index).

**Button semantics (KEY short vs long).** In `main.c`, track KEY press duration. On release: if held < 3s → emit a "previous" switch; if held ≥ 3s → the existing reset already fired at the 3s mark (unchanged). BOOT has no long-press role, so its short-press (on release, or on press-edge) → "next". Switching only changes `current_idx`; the UI reads that index each refresh.

**Index bounds.** `current_idx` wraps with modulo `STOCK_COUNT`. With 3 stocks this is straightforward; with 1 it becomes a no-op (harmless).

**Page indicator.** `stock_ui` renders `[i/N]` (1-based) using ASCII already in the font. No new glyphs for the indicator.

**Font regeneration.** Add `新 乳 业 风 华` to `lv_font_conv --symbols` (科/高 already present). Subset stays small.

## Risks / Trade-offs

- **KEY short/long disambiguation timing.** The reset currently fires the instant 3s is reached (no release needed). Adding short-press-on-release must not accidentally emit a switch right after a reset; since reset calls `esp_restart()`, the loop won't reach the release branch — low risk, but the release logic must guard against emitting a switch when a reset already triggered.
- **Line-to-stock matching.** If a stock code is delisted/invalid, Tencent may omit or return an empty line; matching-by-code keeps the others valid and leaves the missing one on its previous (likely invalid) snapshot.
- **Batched response size.** 3 stocks fit easily in the existing 1023-byte read buffer; more stocks later may require enlarging it (noted for the future web-managed version).
- **Debounce vs edge detection.** Two buttons now drive navigation; press handling must debounce so one physical press = one switch (BSP already debounces 20ms; main.c reads raw gpio for KEY hold — needs consistent edge handling for BOOT).
