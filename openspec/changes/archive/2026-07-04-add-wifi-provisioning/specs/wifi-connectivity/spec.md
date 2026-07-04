## MODIFIED Requirements

### Requirement: Wi-Fi station connection with hardcoded credentials
The system SHALL connect to a Wi-Fi network in station (STA) mode using credentials **stored in NVS at runtime** (not compiled into the firmware), initializing NVS as required by the Wi-Fi stack. When valid credentials are present in NVS, the device SHALL attempt a direct STA connection on boot.

#### Scenario: Successful connection on boot with stored credentials
- **WHEN** the device boots with valid Wi-Fi credentials saved in NVS and is within range of that network
- **THEN** it connects in STA mode, obtains an IP address, and the application proceeds to fetch stock data

#### Scenario: NVS initialized before Wi-Fi
- **WHEN** the Wi-Fi stack is started
- **THEN** `nvs_flash` has been initialized first (erasing and re-initializing if the NVS partition is invalid) so Wi-Fi initialization does not fail

#### Scenario: No stored credentials on boot
- **WHEN** the device boots and no valid credentials exist in NVS
- **THEN** it does not attempt a hardcoded connection and instead enters provisioning (SoftAP captive portal) mode

## REMOVED Requirements

### Requirement: Provisioning deferred
**Reason**: This change implements runtime provisioning, which this requirement explicitly excluded. It is replaced by the "SoftAP captive-portal provisioning", "Persist and reuse credentials", and "Factory reset of credentials" requirements below.
**Migration**: Credentials are no longer compiled in; existing devices with hardcoded credentials must be reflashed once with this change, after which they provision via the portal. The `STOCK_WIFI_SSID` / `STOCK_WIFI_PASSWORD` macros are removed.

## ADDED Requirements

### Requirement: SoftAP captive-portal provisioning
When the device has no usable stored credentials (or a stored network fails to connect), the system SHALL host a Wi-Fi SoftAP and a captive-portal web page so a user can select a network and enter its password from a browser, without a companion app.

#### Scenario: Portal presented when unprovisioned
- **WHEN** the device boots without valid stored credentials
- **THEN** it starts a SoftAP named `Stock_XXXXXX` (suffix derived from the device MAC), open (no password), and serves a web page that lists scanned networks and accepts a password

#### Scenario: DNS hijack triggers captive portal
- **WHEN** a client connects to the SoftAP and issues any DNS/HTTP request
- **THEN** a local DNS server resolves all names to the device, so the phone/OS opens the portal page automatically

#### Scenario: Credentials submitted and connection attempted
- **WHEN** the user submits an SSID and password through the portal
- **THEN** the device attempts to connect in STA mode and reports connecting/connected/failed status back to the portal page

#### Scenario: Portal closes on success
- **WHEN** the device obtains an IP with the submitted credentials
- **THEN** it stops the SoftAP, DNS, and HTTP services, switches to STA-only mode, and proceeds to fetch stock data

### Requirement: Persist and reuse credentials
The system SHALL save successfully-used credentials to NVS and reuse them on subsequent boots so provisioning is a one-time action per network.

#### Scenario: Credentials saved after first success
- **WHEN** a portal-submitted network connects successfully
- **THEN** the SSID and password are written to NVS and used automatically on the next boot without re-showing the portal

#### Scenario: Fallback to portal when saved network unavailable
- **WHEN** stored credentials fail to connect after the boot retry limit
- **THEN** the device falls back to hosting the provisioning portal

### Requirement: Factory reset of credentials
The system SHALL let the user erase stored credentials on-device via a long-press of the KEY button, returning the device to provisioning mode.

#### Scenario: Long-press KEY erases credentials
- **WHEN** the user holds the KEY button for approximately 3 seconds
- **THEN** the device shows a reset indication, erases the stored credentials from NVS, and reboots into provisioning (portal) mode

#### Scenario: Short press does not reset
- **WHEN** the KEY button is pressed briefly (less than the hold threshold)
- **THEN** stored credentials are not erased
