# BambuHelper v2.2 Release Notes

## Settings backup & restore (NEW)

- **Export settings** — download all configuration as a JSON file from the web UI
- **Import settings** — upload a previously exported JSON file to restore configuration after reflashing
- Cloud token is excluded from export for security — re-login required after import
- Partial imports supported (missing fields keep current values)

## Buzzer notifications (NEW, optional)

- **Optional passive buzzer** support on any GPIO pin
- **Print finished** — cheerful ascending melody (C-E-G-C)
- **Print failed** — triple warning beep
- **Quiet hours** — configurable time window to mute buzzer (e.g. 22:00-07:00)
- Disabled by default — enable in Multi-Printer section

## Animated progress bar (NEW)

- **Shimmer effect** — a bright highlight sweeps across the progress bar during printing
- Runs at ~40fps independently of the main display refresh
- Optional — enable via checkbox in Display settings

## Display improvements

- **Background color fix** — custom background colors (e.g. pure black) now apply consistently across all UI elements. Previously, some areas used the default dark navy color creating visible gray bands
- **12h/24h time format** — toggle between 24-hour and 12-hour (AM/PM) clock display

## Web UI reorganization

- **Timezone, DST, and clock format** moved from WiFi section to Display section — changing these no longer requires a device restart
- **Diagnostics** moved to the bottom of the page
- **Chrome download fix** — settings export no longer triggers Chrome's "dangerous file" warning
- Section order: Printer > Display > Multi-Printer > WiFi & System > Diagnostics

## Bug fixes & reliability

- **Firmware version in boot log** — serial output now shows `BambuHelper v2.x Starting` for easier troubleshooting
- **Serial number validation** — empty serial number is now caught before MQTT connection attempt, preventing silent subscription to wrong topic (`device//report`)
- **Printer configuration validation** — web UI warns when required fields (serial, IP, access code) are missing
- **MQTT diagnostic tool** (`tools/mqtt_test.py`) — standalone Python script for testing printer MQTT connectivity outside of BambuHelper

## Build stats

- Flash: 83.3% (1092KB / 1310KB)
- RAM: 14.9% (48.7KB / 328KB)
- Board: lolin_s3_mini (ESP32-S3)
