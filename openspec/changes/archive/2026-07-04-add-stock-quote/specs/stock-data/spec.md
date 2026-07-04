## ADDED Requirements

### Requirement: Poll the realtime quote endpoint on a fixed interval
The system SHALL fetch the realtime quote for stock `002859` from the Tencent endpoint `http://qt.gtimg.cn/q=sz002859` over plain HTTP every 5 seconds.

#### Scenario: Periodic fetch
- **WHEN** the device is connected to Wi-Fi
- **THEN** it issues an HTTP GET to the endpoint approximately every 5 seconds

#### Scenario: Plain HTTP, no key
- **WHEN** the request is made
- **THEN** it uses plain HTTP (no TLS) and requires no API key or authentication

### Requirement: Parse the delimited quote record
The system SHALL parse the `~`-delimited response record and extract the current price, previous close, open, high, low, change amount, change percent, and update timestamp by field position.

#### Scenario: Fields extracted from a valid record
- **WHEN** a well-formed record is received (e.g. `v_sz002859="...~93.27~98.91~93.00~...~-5.64~-5.70~97.20~90.01~..."`)
- **THEN** the numeric fields are extracted into typed values (price, prev-close, open, high, low, change amount, change %) and an update timestamp

#### Scenario: Defensive parsing of malformed data
- **WHEN** the response is truncated, empty, or has fewer fields than expected
- **THEN** the parser reports failure without crashing, and does not overwrite previously good values with garbage

### Requirement: Ignore the GBK name field
The system SHALL NOT decode the GBK-encoded name field from the response; the displayed Chinese name is provided separately as a hardcoded UTF-8 constant.

#### Scenario: Raw bytes split safely
- **WHEN** the response containing GBK bytes is parsed
- **THEN** parsing splits on the ASCII `~` delimiter and reads only ASCII numeric fields, never decoding the GBK name bytes

### Requirement: Fault tolerance on fetch failure
The system SHALL keep operating when a fetch fails (timeout, connection error, or parse failure), retaining the last known values or indicating unavailable data.

#### Scenario: Transient failure
- **WHEN** a single fetch times out or fails to parse
- **THEN** the poll loop continues, the app does not crash or reboot, and the UI shows `--` or the last known values
