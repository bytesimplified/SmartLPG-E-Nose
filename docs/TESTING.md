# Bench testing and calibration

This project is a demonstration of **relative MQ sensor signals**. The rules in the sketch are illustrative and are **not validated leak thresholds**, ppm estimates, or %LEL calculations.

## Tooling
- Arduino IDE 2.x with Espressif ESP32 board package
- Classic **ESP32 Dev Module** selected
- USB data cable
- Serial Monitor at **115200 baud**

## Steps
1. After verifying wiring, open `firmware/LPG_ENose_ESP32_Kitchen/LPG_ENose_ESP32_Kitchen.ino`.
2. Leave `WIFI_SSID` empty for offline mode; the local audible/visual indicators do not depend on Wi-Fi.
3. Upload to the ESP32 and monitor diagnostics.
4. Allow the restart warm-up (180 seconds); this **does not replace** the sensor manufacturer's initial burn-in and conditioning requirements.
5. Only in **verified clean air**, type `c` in Serial Monitor to collect a 60-sample baseline; this is stored in ESP32 preferences.
6. Type `t` to inject a **simulated** alarm state. This safely exercises local indicators without LPG.
7. Type `r`, or press the reset button, only after the simulated event clears and the code's reset interval has elapsed.
8. If Wi-Fi is configured and joins the same local network, open the IP address printed in Serial Monitor. `/api` exposes JSON with statuses and sensor voltages.

## Expected outcomes

| Scenario | Expected prototype behavior |
| --- | --- |
| Boot/restart | Not-ready indication through warm-up |
| No saved baseline | Calibration-required indication |
| `c` in verified clean air | Baseline collection and save |
| `t` after warm-up and calibration | Latched alarm; buzzer/red LED pulses |
| Wi-Fi unavailable | Local alarm logic continues |
| ADC signal near zero for consecutive samples | Sensor-fault warning |
| `r` too early | Reset rejected |
| `r` after clear interval | Latched alarm cleared |

## Limitations to record in a project report
- No validated LPG discrimination from alcohol, smoke, humidity, or cooking vapors
- No calibrated ppm or lower explosive limit measurement
- MQ devices have response delays, environmental drift, and heater conditioning needs
- Some wiring failures (including stuck-high channels) may not be recognized
- A power outage prevents operation unless externally managed; no redundant power
- Dashboard only works on local Wi-Fi and has **no authentication**
- No SMS, Telegram, push notifications, or secure remote telemetry implemented
- Not independently verified on actual ESP32 hardware

## Test safety
**Do not release LPG indoors, near a flame, or in an occupied kitchen to test the project.** Any exposure-response evaluation requires a professionally supervised, suitable laboratory with controlled concentrations and protective equipment. Use the `t` simulation for classroom demonstrations.
