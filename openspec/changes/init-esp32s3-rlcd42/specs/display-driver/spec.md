## ADDED Requirements

### Requirement: ST7305 reflective LCD driver over SPI
The system SHALL drive the 4.2" reflective monochrome LCD via an ST7305 controller over SPI, using the initialization sequence appropriate for this panel.

#### Scenario: Display initializes without error
- **WHEN** the BSP initializes the display
- **THEN** the ST7305 controller receives its init/reset sequence over SPI and the panel is ready to accept frame data without runtime errors

#### Scenario: Test pattern renders correctly
- **WHEN** a known test pattern (e.g., border and checkerboard) is written to the panel on hardware
- **THEN** the pattern appears correctly on the reflective LCD, confirming SPI wiring and pixel packing

### Requirement: Monochrome pixel packing
The driver SHALL pack framebuffer pixels into the 1-bit-per-pixel format expected by the ST7305 addressing mode before transmitting over SPI.

#### Scenario: 1-bpp buffer transmitted
- **WHEN** a full-screen buffer is flushed to the display
- **THEN** pixels are packed to 1 bit per pixel in the ST7305 page/column layout and the visible output matches the intended image

### Requirement: LVGL integration at 1-bit color depth
The display driver SHALL be registered as an LVGL display so the application renders through LVGL configured for monochrome (1-bit) output.

#### Scenario: LVGL draws to the panel
- **WHEN** an LVGL widget is created and the LVGL timer runs
- **THEN** LVGL's flush callback writes the rendered region to the ST7305 panel and the widget is visible on the reflective LCD

#### Scenario: LVGL configured for monochrome
- **WHEN** the LVGL build configuration is inspected
- **THEN** color depth is set to 1 bit to match the black/white panel
