# stock-ui Specification

## Purpose
TBD - created by archiving change add-stock-quote. Update Purpose after archive.
## Requirements
### Requirement: Render the single-stock quote screen
The system SHALL render an LVGL screen showing the hardcoded name "洁美科技", the code `002859`, the current price, the change percent, the open/high/low/previous-close values, and the last-update time on the monochrome panel.

#### Scenario: Quote displayed after first successful fetch
- **WHEN** the first valid quote record is fetched and parsed
- **THEN** the screen shows 洁美科技, 002859, current price, change %, open/high/low/prev-close, and the update timestamp

#### Scenario: UI updates under the LVGL lock
- **WHEN** new quote values are applied to the screen
- **THEN** all `lv_*` calls occur between `bsp_lvgl_lock()` and `bsp_lvgl_unlock()`

### Requirement: Monochrome up/down indication
The system SHALL indicate rising vs falling price using symbols (e.g. ↑/↓), not color, because the panel is black/white.

#### Scenario: Direction shown without color
- **WHEN** the change percent is positive or negative
- **THEN** an ↑ or ↓ (or equivalent glyph) is shown alongside the change value, with no reliance on red/green

### Requirement: Embedded Chinese font subset
The system SHALL render Chinese text using a 1-bpp font subset embedded in the firmware that contains only the fixed glyphs the UI displays, declared via `LV_FONT_DECLARE`, without modifying the managed LVGL component.

#### Scenario: Fixed glyphs render correctly
- **WHEN** the screen displays 洁美科技 and the fixed Chinese labels
- **THEN** those glyphs render from the embedded 1-bpp subset font

#### Scenario: No dynamic Chinese required
- **WHEN** the UI needs Chinese text
- **THEN** all such text is compile-time-fixed, so the subset font is sufficient and no runtime GBK decoding occurs

### Requirement: No-data / connecting state
The system SHALL present a clear state when data is unavailable, distinguishing **provisioning mode**, Wi-Fi connecting, and no-data-yet, so the user always knows what the device needs.

#### Scenario: Before data / on failure
- **WHEN** Wi-Fi is connecting or no valid quote has been fetched yet
- **THEN** the screen shows a status such as "连接中" or `--` placeholders instead of stale or blank fields

#### Scenario: Provisioning mode guidance shown
- **WHEN** the device is in provisioning (SoftAP portal) mode
- **THEN** the screen shows the hotspot name (matching the actual `Stock_XXXXXX` AP SSID) and the portal URL (`192.168.4.1`) so the user knows what to connect to

#### Scenario: Reset feedback shown
- **WHEN** the user completes the long-press factory-reset gesture
- **THEN** the screen shows a reset indication (e.g. "重置中") before the device reboots

### Requirement: Provisioning-screen glyphs in the font subset
The embedded 1-bpp Chinese font subset SHALL include the fixed glyphs needed by the provisioning and reset screens, keeping all displayed Chinese compile-time-fixed.

#### Scenario: Provisioning text renders
- **WHEN** the provisioning or reset screen is shown
- **THEN** its Chinese labels (e.g. 配网 / 请连接 / 热点 / 浏览器 / 打开 / 重置) render from the embedded subset font with no missing glyphs

