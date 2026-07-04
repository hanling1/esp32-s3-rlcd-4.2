## Why

The device shows a single hardcoded stock (002859 洁美科技). Users want to watch a small set of self-selected stocks and flip between them on-device without recompiling per view. A hardcoded watchlist plus left/right key switching delivers that with minimal risk, and lays the data-layer groundwork for a future web-managed list.

## What Changes

- Replace the single-stock config with a small hardcoded watchlist of 3 stocks: `002859 洁美科技`, `002946 新乳业`, `000636 风华高科`.
- Poll all watchlist stocks in ONE batched Tencent request (`q=sz002859,sz002946,sz000636`) and parse each returned line into a per-stock snapshot.
- Store an array of quotes instead of a single quote; expose them by index.
- Add on-device switching: KEY (left, gpio18) short-press = previous stock, BOOT (right, gpio0) short-press = next stock, wrapping around. KEY long-press 3s still triggers the existing Wi-Fi credential reset.
- Show a page indicator (e.g. `[1/3]`) on the monochrome screen.
- Extend the 1-bpp subset font with the glyphs for the new stock names (新 乳 业 风 华).

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `stock-data`: fetching/parsing changes from one stock to a batched multi-stock watchlist, stored and retrieved by index.
- `stock-ui`: the screen now renders a selectable current stock with a page indicator, and the KEY/BOOT buttons switch the selection (KEY short-press vs long-press disambiguation).

## Impact

- `components/stock_app/include/stock_config.h`: watchlist table replaces the single-stock scalar macros.
- `components/stock_app/include/stock_data.h` + `src/stock_data.c`: quote array, index-based getter, batched fetch + multi-line parse.
- `main/main.c`: current-index state; KEY short/long-press disambiguation; BOOT short-press switching.
- `components/stock_app/src/stock_ui.c`: render current stock + `[i/N]` page indicator.
- `components/stock_app/src/font_stock_16.c`: regenerated subset font with the added name glyphs.
- No new dependencies; no change to the Wi-Fi provisioning flow other than sharing the KEY button (long-press reset preserved).
