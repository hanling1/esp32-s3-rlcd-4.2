## MODIFIED Requirements

### Requirement: Render the single-stock quote screen
The system SHALL render an LVGL screen showing the CURRENTLY SELECTED watchlist stock: its UTF-8 name, its code, the current price, the change percent, the open/high/low/previous-close values, the last-update time, and a page indicator `[i/N]` (1-based position within the watchlist), on the monochrome panel.

#### Scenario: Selected stock displayed after a successful fetch
- **WHEN** a valid quote record has been fetched for the selected stock
- **THEN** the screen shows that stock's name, code, current price, change %, open/high/low/prev-close, update timestamp, and the `[i/N]` page indicator

#### Scenario: Screen follows the current selection
- **WHEN** the current selection index changes
- **THEN** the next UI refresh renders the newly selected stock's values and updates the `[i/N]` indicator

#### Scenario: UI updates under the LVGL lock
- **WHEN** new quote values are applied to the screen
- **THEN** all `lv_*` calls occur between `bsp_lvgl_lock()` and `bsp_lvgl_unlock()`

## ADDED Requirements

### Requirement: On-device watchlist switching via buttons
The system SHALL let the user switch the selected stock using the two readable buttons: a short press of the KEY button (gpio18, left) selects the PREVIOUS stock and a short press of the BOOT button (gpio0, right) selects the NEXT stock, wrapping around the watchlist. This SHALL coexist with the existing KEY long-press (~3s) Wi-Fi credential reset.

#### Scenario: Short press switches selection
- **WHEN** the user briefly presses KEY (left) or BOOT (right)
- **THEN** the selection moves to the previous or next watchlist stock respectively, wrapping at the ends

#### Scenario: Long-press still resets Wi-Fi, not switch
- **WHEN** the user holds KEY for approximately 3 seconds
- **THEN** the Wi-Fi credential reset triggers as before, and no stock-switch is emitted for that gesture

#### Scenario: One press, one step
- **WHEN** a single physical button press occurs
- **THEN** the selection advances by exactly one stock (debounced, no repeats from a single press)

### Requirement: Watchlist name glyphs in the font subset
The embedded 1-bpp Chinese font subset SHALL include the fixed glyphs for every watchlist stock's display name, keeping all displayed Chinese compile-time-fixed.

#### Scenario: All watchlist names render
- **WHEN** each watchlist stock is selected and displayed
- **THEN** its Chinese name renders from the embedded subset font with no missing glyphs
