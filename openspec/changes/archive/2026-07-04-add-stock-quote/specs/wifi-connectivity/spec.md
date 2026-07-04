## ADDED Requirements

### Requirement: Wi-Fi station connection with hardcoded credentials
The system SHALL connect to a Wi-Fi network in station (STA) mode using credentials compiled into the firmware (SSID `solaso_5G`), initializing NVS as required by the Wi-Fi stack.

#### Scenario: Successful connection on boot
- **WHEN** the device boots within range of the configured network
- **THEN** it connects in STA mode and obtains an IP address, and the application proceeds to fetch stock data

#### Scenario: NVS initialized before Wi-Fi
- **WHEN** the Wi-Fi stack is started
- **THEN** `nvs_flash` has been initialized first (erasing and re-initializing if the NVS partition is invalid) so Wi-Fi initialization does not fail

### Requirement: Automatic reconnection and visible status
The system SHALL retry the connection when it is not established or is lost, and SHALL expose a connection status the UI can display.

#### Scenario: Retry while disconnected
- **WHEN** the network is unavailable or the connection drops
- **THEN** the device keeps retrying without blocking the UI task, and the UI can show a "connecting"/"failed" state

#### Scenario: Recovery after network returns
- **WHEN** the configured network becomes available again after a disconnect
- **THEN** the device reconnects automatically and stock polling resumes

### Requirement: Provisioning deferred
The system SHALL treat Wi-Fi provisioning (runtime SSID/password configuration) as out of scope for this change; credentials are hardcoded.

#### Scenario: No provisioning flow in v1
- **WHEN** the device has no network configured other than the hardcoded credentials
- **THEN** it does not present a provisioning UI and relies solely on the compiled-in SSID/password
