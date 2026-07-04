## ADDED Requirements

### Requirement: Physical button input handling
The system SHALL read the three physical buttons (BOOT, PWR, KEY) as the primary input method, since the board has no touchscreen.

#### Scenario: Button press detected
- **WHEN** a user presses the KEY button
- **THEN** the firmware detects the press event with debouncing and makes it available to the application

#### Scenario: No touch driver present
- **WHEN** the input subsystem is initialized
- **THEN** no touchscreen driver is initialized and input relies solely on the physical buttons

### Requirement: Button input exposed to the application/LVGL
The button input SHALL be surfaced to the application layer so LVGL or app logic can respond to presses.

#### Scenario: Application reacts to a button
- **WHEN** the KEY button is pressed while the sample app is running
- **THEN** the application observes the event and can update the UI in response

#### Scenario: Debounced, no false triggers
- **WHEN** a single physical press occurs
- **THEN** exactly one logical press event is delivered (no bounce-induced duplicates)
