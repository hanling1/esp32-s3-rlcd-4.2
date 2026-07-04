## ADDED Requirements

### Requirement: Sample LVGL status screen
The application SHALL render a minimal LVGL screen on startup that displays static content (e.g., a title/status label) to prove the display pipeline end-to-end.

#### Scenario: Screen shows on boot
- **WHEN** the firmware is flashed and the board powers on
- **THEN** the reflective LCD displays the sample LVGL screen with legible monochrome content within a few seconds of boot

### Requirement: Sample app responds to button input
The sample application SHALL update its UI in response to a physical button press to prove the input pipeline end-to-end.

#### Scenario: UI updates on KEY press
- **WHEN** the user presses the KEY button while the sample screen is displayed
- **THEN** the visible UI changes (e.g., a counter increments or a label toggles), confirming input reaches the UI

### Requirement: End-to-end build and flash verification
The sample app SHALL be buildable and flashable with standard ESP-IDF commands, serving as the verification of the whole scaffold.

#### Scenario: Build succeeds without hardware
- **WHEN** a developer runs `idf.py build`
- **THEN** the build completes with exit code 0 even without a board connected

#### Scenario: Flash and run on hardware
- **WHEN** a developer runs `idf.py -p <port> flash monitor` with the board connected
- **THEN** the firmware boots, the sample screen appears, and no error/reboot loop is observed in the serial monitor
