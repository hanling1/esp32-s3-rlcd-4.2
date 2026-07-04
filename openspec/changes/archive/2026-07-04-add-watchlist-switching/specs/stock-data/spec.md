## MODIFIED Requirements

### Requirement: Poll the realtime quote endpoint on a fixed interval
The system SHALL fetch realtime quotes for all stocks in a hardcoded watchlist from the Tencent endpoint in a SINGLE batched request over plain HTTP every 5 seconds, using a comma-separated secid query (e.g. `http://qt.gtimg.cn/q=sz002859,sz002946,sz000636`).

#### Scenario: Periodic batched fetch
- **WHEN** the device is connected to Wi-Fi
- **THEN** it issues one HTTP GET containing all watchlist secids approximately every 5 seconds

#### Scenario: Plain HTTP, no key
- **WHEN** the request is made
- **THEN** it uses plain HTTP (no TLS) and requires no API key or authentication

### Requirement: Parse the delimited quote record
The system SHALL parse EACH `~`-delimited response line returned for the batched request and extract the current price, previous close, open, high, low, change amount, change percent, and update timestamp by field position, associating each parsed line with its watchlist stock.

#### Scenario: Fields extracted from each valid line
- **WHEN** a batched response contains one `v_...="...";` line per requested stock
- **THEN** each line's numeric fields are extracted into typed values for the corresponding stock, keyed by its code

#### Scenario: Defensive parsing of malformed data
- **WHEN** a line is truncated, empty, missing, or has fewer fields than expected
- **THEN** the parser reports failure for that stock without crashing, and does not overwrite that stock's previously good values with garbage

### Requirement: Fault tolerance on fetch failure
The system SHALL keep operating when a fetch fails (timeout, connection error, or parse failure), retaining the last known values per stock or indicating unavailable data.

#### Scenario: Transient failure
- **WHEN** a single batched fetch times out or fails to parse
- **THEN** the poll loop continues, the app does not crash or reboot, and each stock's UI shows `--` or its last known values

## ADDED Requirements

### Requirement: Hardcoded watchlist of multiple stocks
The system SHALL define a compile-time watchlist of stocks, each with a Tencent secid, a display code, and a UTF-8 display name, as the single source of truth for what is polled and shown.

#### Scenario: Watchlist drives polling and display
- **WHEN** the firmware is built with a watchlist of N stocks
- **THEN** all N are polled together and each is individually selectable for display

#### Scenario: Names are compile-time fixed
- **WHEN** a watchlist entry specifies a Chinese display name
- **THEN** that name is a compile-time UTF-8 constant whose glyphs are present in the embedded font subset (no runtime GBK decoding)

### Requirement: Index-based quote storage and retrieval
The system SHALL store one quote snapshot per watchlist stock and expose them by index under a mutex, so a consumer can read a consistent snapshot for a selected stock.

#### Scenario: Read a stock by index
- **WHEN** a consumer requests the quote at a valid index
- **THEN** it receives a consistent copy of that stock's latest snapshot taken under the lock

#### Scenario: Watchlist count is queryable
- **WHEN** a consumer needs to bound or wrap a selection index
- **THEN** the number of watchlist stocks is available to it
