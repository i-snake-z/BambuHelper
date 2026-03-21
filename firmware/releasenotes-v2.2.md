# BambuHelper v2.2 Release Notes

## Settings backup & restore (NEW)

- **Export settings** - download all configuration as a JSON file from the web UI
- **Import settings** - upload a previously exported JSON file to restore configuration after reflashing
- Cloud token is excluded from export for security - re-login required after import
- Partial imports supported (missing fields keep current values)

## Buzzer notifications (NEW, optional)

- **Optional passive buzzer** support on any GPIO pin
- **Print finished** - cheerful ascending melody (C-E-G-C)
- **Print failed** - triple warning beep
- **Quiet hours** - configurable time window to mute buzzer (e.g. 22:00-07:00)
- Disabled by default - enable in Multi-Printer section

## Animated progress bar (NEW)

- **Shimmer effect** - a bright highlight sweeps across the progress bar during printing
- Runs at ~40fps independently of the main display refresh
- Optional - enable via checkbox in Display settings

## Pong clock (NEW, optional)

- **Breakout-style animated clock** - classic Arcanoid game plays on the clock screen
- Colored bricks, bouncing ball, auto-tracking paddle
- On minute change, digits break apart with fragment explosion effects and bounce back
- Runs at ~50fps independently of the main display refresh
- Optional - enable via checkbox in Display settings

## Automatic DST (NEW)

- **Timezone regions** replace the old UTC offset dropdown - select your region (e.g. "Central European (Poland, Germany)") and DST switches automatically
- No more manual DST checkbox - POSIX timezone rules handle transitions
- Existing settings migrate automatically on first boot after update
- 48 regions covering Europe, Americas, Asia-Pacific, Middle East & Africa

## Display improvements

- **12h/24h time format** - toggle between 24-hour and 12-hour (AM/PM) clock display

## Web UI improvements

- **Timezone and clock format** moved from WiFi section to Display section - changing these no longer requires a device restart
- **Show WiFi password** - checkbox to reveal password when entering WiFi credentials
- **Diagnostics** moved to the bottom of the page
- **Chrome download fix** - settings export no longer triggers Chrome's "dangerous file" warning
- Section order: Printer > Display > Multi-Printer > WiFi & System > Diagnostics

## Bug fixes & reliability

- **Connecting screen text alignment** - connection info (mode, serial/IP, elapsed time) was rendered off-center due to a text datum bug; now properly centered with larger font
- **Gauge Colors card styling** - Gauge Colors section in web UI was missing card border/padding due to a premature closing `</div>`; now renders inside the Display card correctly
- **Background color fix** - custom background colors (e.g. pure black) now apply consistently across all UI elements; previously some areas used the default dark navy color creating visible gray bands
- **Firmware version in boot log** - serial output now shows `BambuHelper v2.x Starting` for easier troubleshooting
- **Serial number validation** - empty serial number is now caught before MQTT connection attempt, preventing silent subscription to wrong topic (`device//report`)
- **Printer configuration validation** - web UI warns when required fields (serial, IP, access code) are missing
- **MQTT diagnostic tool** (`tools/mqtt_test.py`) - standalone Python script for testing printer MQTT connectivity outside of BambuHelper

## Build stats

- Flash: 83.3% (1092KB / 1310KB)
- RAM: 14.9% (48.7KB / 328KB)
- Board: lolin_s3_mini (ESP32-S3)
