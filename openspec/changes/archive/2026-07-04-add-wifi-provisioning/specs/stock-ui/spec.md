## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Provisioning-screen glyphs in the font subset
The embedded 1-bpp Chinese font subset SHALL include the fixed glyphs needed by the provisioning and reset screens, keeping all displayed Chinese compile-time-fixed.

#### Scenario: Provisioning text renders
- **WHEN** the provisioning or reset screen is shown
- **THEN** its Chinese labels (e.g. 配网 / 请连接 / 热点 / 浏览器 / 打开 / 重置) render from the embedded subset font with no missing glyphs
