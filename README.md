# SmartLPG-E-Nose

**ESP32-based LPG gas-sensing educational prototype** for home-kitchen research. This project combines MQ-6 and MQ-2 analog signals, a local buzzer and LEDs, manually stored calibration, and an optional Wi-Fi dashboard.

> **Not a certified household gas alarm.** Use a separate certified LPG detector, installed according to its manufacturer's guidance. Never deliberately release LPG indoors to test this prototype.

## Project files

| File | Purpose |
| --- | --- |
| firmware/LPG_ENose_ESP32_Kitchen/LPG_ENose_ESP32_Kitchen.ino | Arduino firmware (in separate firmware PR) |
| [docs/WIRING.md](docs/WIRING.md) | Wiring, ADC protection, mounting/power cautions |
| [docs/TESTING.md](docs/TESTING.md) | Calibration, simulation test, and limitations |

## Architecture

MQ-6 AO → 10 kΩ / 10 kΩ voltage divider → ESP32 GPIO34

MQ-2 AO → separate 10 kΩ / 10 kΩ voltage divider → ESP32 GPIO35

ESP32 → local LED indicators + active buzzer driver; optionally Wi-Fi → local browser dashboard and /api JSON endpoint.

Both MQ modules require an appropriate regulated 5 V supply and a common ground. **Never wire a 5 V analog output straight to an ESP32 ADC pin.**

## Getting started

1. Read [WIRING.md](docs/WIRING.md) and assemble the prototype on a bench.
2. Install Arduino IDE 2.x and the Espressif ESP32 board package; use a classic ESP32 DevKit.
3. Open the Arduino sketch in its matching folder when the firmware PR has been merged.
4. Set optional Wi-Fi SSID/password in the sketch, or leave the SSID empty for Serial-only use.
5. Upload and open Serial Monitor at 115200 baud.
6. Complete MQ sensor manufacturer-recommended initial conditioning. A restart warm-up in the sketch is **not** equivalent to initial burn-in.
7. In **verified clean air**, type c to collect a baseline. Type t to simulate a local alarm without gas; type r to request reset after the clear interval.

See [TESTING.md](docs/TESTING.md) for more detail.

## Technical and safety limitations

The firmware applies **illustrative relative sensor-voltage thresholds**, not validated LPG-specific concentration measurements. It does not calculate ppm or %LEL. Interfering vapors, humidity, heater drift, power failures, and unrecognized wiring faults can cause missed events or false positives. The web page is LAN-only, lacks authentication, and does not send phone push/SMS alerts. Never use this board for DIY mains switching, gas-valve actuation, or safety interlocks.

**DNA sequences are not implemented as physical sensors in this revision.** A DNA-functionalized E-Nose requires fabricated sensing elements and independent experimental validation.

## Contributing

Use small focused pull requests. Keep simulation, calibrated measurement, and certified safety claims clearly separated.
