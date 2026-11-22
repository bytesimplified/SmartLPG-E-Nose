# Local research dashboard

The dashboard shows MQ-6/MQ-2 **estimated analog voltage**, SHT31 temperature and humidity (when available), device/fault status, experiment activity labels and a live browser-side chart. Readings are not calibrated to LPG concentration or %LEL. This is **not** a replacement for an independent certified alarm.

## Set up local credentials

Copy `secrets.example.h` to `secrets.h` in the same firmware folder and enter your own values for:

- `ENOSE_WIFI_SSID`, `ENOSE_WIFI_PASSWORD`
- `ENOSE_DASHBOARD_USER`, `ENOSE_DASHBOARD_PASSWORD` (at least 12 characters)

`secrets.h` is listed in `.gitignore`. Verify with `git status` that actual credentials are not committed. If Wi-Fi is not configured, the ESP32 runs Serial-only; its local LED/buzzer logic still works. If Wi-Fi is configured but dashboard credentials are missing, HTTP requests are refused.

After connecting, the serial console shows a local IP address. On a device on the **same trusted Wi-Fi network**, open that IP address in a browser. Use HTTP Basic authentication when prompted.

**Security limitations:** HTTP Basic is not HTTPS; credentials and readings are not end-to-end encrypted. Wi-Fi encryption and LAN isolation are important. Do not forward the device's port to the Internet, enable UPnP access, expose it to public/hotel Wi-Fi, or treat it as a remotely secure monitoring service. ESP32 HTTP server, network connectivity and browsers may fail; this dashboard is not the primary alarm.

## Dashboard elements

- **Local status and sensor-health warnings**: status text (including not-ready/fault) and warning styles
- **MQ-6/MQ-2 trend line chart**: last 120 browser polls at 2-second intervals, cleared when the page is reloaded or device uptime decreases
- **Environment**: temperature and humidity with unavailable indicators for SHT31 errors
- **CSV export**: downloads the latest 300 ESP32 RAM-buffer records (about 5 minutes at 1 Hz) from `/csv`
- **JSON API**: `/api` including `uptime_ms`, `mq6_mV`, `mq2_mV`, `temperature_c`, `humidity_pct`, `env_valid`, `alarm`, `fault`, `fault_reason`, `recorded_rows` and `activity_label`

For longer recorded sessions, use [the USB serial collector](DATA_LOGGING.md). Neither the CSV export nor the dashboard creates a permanent onboard database.

## Manual smoke check (without LPG)

1. Connect the module and launch the dashboard after following the wiring and sensor conditioning guides.
2. Verify readings refresh and the chart accumulates points when the page stays open.
3. Temporarily unplug only the optional **SHT31** module in a supervised low-risk lab bench setup; temperature/humidity should report unavailable while MQ measurement and local warning remain independent.
4. Download `/csv` after several samples; check it contains a header and sequential `uptime_ms` values.
5. Use serial `t` to simulate the alarm after warm-up/calibration and verify the warning appears in the next poll. Never release LPG indoors to generate a test.
6. Check that unauthenticated `/api` and `/csv` requests are rejected on the configured local server.

A compiled build and local mock testing do not prove gas detection effectiveness, electrical safety, or fail-safe operation.
