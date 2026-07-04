## ADDED Requirements

### Requirement: ESP-IDF project targets ESP32-S3 with correct memory configuration
The project SHALL build as an ESP-IDF v5.x application targeting the `esp32s3` chip, configured for 16 MB flash and 8 MB octal PSRAM at 80 MHz to match the ESP32-S3-WROOM-1-N16R8 module.

#### Scenario: Project builds successfully
- **WHEN** a developer runs `idf.py set-target esp32s3` followed by `idf.py build`
- **THEN** the build completes with exit code 0 and produces a flashable binary

#### Scenario: PSRAM and flash configured for the module
- **WHEN** the effective sdkconfig is inspected after build
- **THEN** octal PSRAM is enabled at 80 MHz and the flash size is set to 16 MB

### Requirement: Centralized board pin mapping
The BSP SHALL expose all board GPIO assignments (display SPI CLK/MOSI/CS/DC/RESET, and BOOT/PWR/KEY buttons) from a single header so pin corrections are made in one place.

#### Scenario: Pins referenced from one header
- **WHEN** a developer needs a GPIO number for any supported peripheral
- **THEN** it is defined as a named constant in the single BSP pin-map header and not hardcoded elsewhere

#### Scenario: Pins verified against schematic
- **WHEN** the BSP is finalized
- **THEN** every GPIO constant in the pin-map header has been confirmed against the official ESP32-S3-RLCD-4.2 schematic

### Requirement: BSP initialization entry point
The BSP SHALL provide a single initialization function that brings up the core peripherals (display and buttons) required by the sample application.

#### Scenario: Application initializes the board via BSP
- **WHEN** the application calls the BSP init function at startup
- **THEN** the display and button subsystems are initialized and ready for use without the application touching low-level SPI/GPIO setup directly
