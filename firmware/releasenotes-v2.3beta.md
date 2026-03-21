# BambuHelper v2.3 Beta Release Notes

## AMS filament indicator (NEW)

- **Active filament on screen** - colored dot + filament type (e.g. "PLA Matte") shown on the bottom bar during PRINTING and IDLE screens
- Works with all AMS units (up to 4 units, 16 trays) and external spool (vt_tray)
- Black filament visibility - gray outline around the color dot so it's visible on dark backgrounds
- Falls back to WiFi signal display when no AMS is present or no tray is active
- Data parsed from MQTT pushall using memmem() raw scan (same proven pattern as H2C dual nozzle)

## Buzzer improvements

- **Test buzzer button** - cycle through all sounds (Print Finished, Error, Connected) directly from the web UI
- **Section renamed** - "Multi-Printer" section renamed to "Hardware & Multi-Printer" for discoverability

## Display fixes

- **Pong clock text size bug** - switching from Pong clock to printer dashboard no longer shows garbled oversized text (tft.setTextSize was not reset)
- **ETA fallback fix** - ETA display no longer intermittently falls back to "Remaining: Xh XXm" after DST implementation (race condition in getLocalTime with timeout 0)
- **Bottom bar font upgrade** - bottom status bar changed from Font 1 (8px) to Font 2 (16px) for better readability
- **Default background color** - changed from dark navy (0x0861) to black (0x0000)

## Build stats

- Flash: 84.3% (1104KB / 1310KB)
- RAM: 15.6% (51KB / 328KB)
- Board: lolin_s3_mini (ESP32-S3)
