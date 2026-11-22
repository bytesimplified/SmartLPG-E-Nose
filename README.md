# SmartLPG-E-Nose

**SmartLPG-E-Nose** is an open-source, ESP32-based **electronic-nose research prototype** for studying LPG-responsive gas-sensor signals in home-kitchen environments. It combines two metal-oxide semiconductor (MOS) gas sensors with optional environmental measurements, local warning indicators, time-series logging, offline analysis, and a browser dashboard.

> [!WARNING]
> **Research/educational prototype only — not a certified LPG detector.** Do not rely on this system for household gas safety. Install and maintain a separate certified LPG alarm according to its manufacturer's instructions. Never deliberately release LPG indoors for testing.

## Project objective

Explore whether combining multiple gas-responsive sensors, environmental observations, and reproducible data analysis can help characterize gas-sensor behavior and reduce nuisance responses during everyday kitchen activities. The project is designed for **measurement and experimentation**, not for validated leak-concentration measurement.

## System overview

```text
 MQ-6 LPG-responsive sensor ── voltage divider ── GPIO34 ┐
                                                         │
 MQ-2 general gas sensor ── voltage divider ─── GPIO35 ──┼── ESP32
                                                         │     ├── Buzzer + red/green LEDs
 SHT31 temperature/humidity (optional) ─── I2C 21/22 ───┘     ├── Serial CSV stream
                                                               ├── Local /api and /csv
                                                               └── Optional LAN dashboard
                                                                     │
                                                                     └── Offline Python analysis
```

**Important distinction:** MQ-2 and MQ-6 measure changes in the electrical response of heated metal-oxide sensing elements; they do not detect the odorant by smelling it. This version does **not** contain DNA-functionalized sensing hardware. The displayed MQ values are approximate analog-output **millivolts**, not LPG ppm or %LEL.

## Capabilities

| Area | Implementation |
| --- | --- |
| **Gas-responsive sensing** | MQ-6 + MQ-2 analog signals through independent 10 kΩ / 10 kΩ voltage dividers |
| **Environmental monitoring** | Optional SHT31 temperature and relative humidity over I²C with CRC-checked readings |
| **Signal processing** | Averaging, manual clean-air baseline, illustrative relative-rise classifier and consecutive-sample confirmation |
| **Sensor diagnostics** | Heuristic low-signal, near-rail and possible flatline warnings; no complete hardware self-test |
| **Local indicators** | Red/green LEDs and a buzzer; signal-warning behavior operates without Wi-Fi |
| **Logging** | 1 Hz USB Serial CSV readings, activity labels, optional host UTC timestamps, and a 300-row RAM buffer |
| **Local dashboard** | Live MQ trends, environmental values, device state, local JSON API and CSV download |
| **Offline research** | Python feature extraction and Random Forest comparison using independent, session-held-out datasets |
| **Build/test automation** | Arduino compilation, host-side C++ tests, Python CSV parser tests and ML feature tests with GitHub Actions |

The offline models compare **manually annotated background, cooking and cleaning activities**. They are not trained or validated as LPG leak classifiers, and they do not control the device's alarm logic.

## Hardware and wiring

- Classic ESP32 DevKit (ESP32-WROOM-32 family)
- MQ-6 module: analog output via a **10 kΩ / 10 kΩ divider** to **GPIO34**
- MQ-2 module: analog output via its own **10 kΩ / 10 kΩ divider** to **GPIO35**
- Optional SHT31 module: **SDA GPIO21 / SCL GPIO22**, with **3.3 V-compatible I²C pull-ups**
- Active buzzer control on **GPIO25** (use an appropriate driver when required)
- Red LED **GPIO26**, green LED **GPIO27**, push-button reset **GPIO33**
- Correctly rated regulated sensor-heater supply, ESP32 supply, common ground and current-limiting resistors

**Never connect a potentially 5 V MQ analog output directly to an ESP32 ADC pin.** Check module specifications and supply demands before powering the circuit. Read [Wiring & hardware](docs/WIRING.md) and [Environmental sensor setup](docs/ENVIRONMENT.md).

## Getting started

1. Assemble and verify the prototype on a **safe laboratory bench** following [WIRING.md](docs/WIRING.md); keep a separate certified LPG alarm for any home use.
2. Install [Arduino IDE](https://www.arduino.cc/en/software) and the Espressif ESP32 board package; choose a compatible classic **ESP32 Dev Module**.
3. Open [the Arduino firmware](firmware/LPG_ENose_ESP32_Kitchen/LPG_ENose_ESP32_Kitchen.ino), compile and upload it.
4. Open Serial Monitor at **115200 baud**. Follow the MQ module manufacturer's initial heater conditioning requirements; the sketch's **3-minute restart warm-up is not initial burn-in**.
5. In **verified clean air only**, send `c` for manual baseline calibration. Use `t` to exercise a **simulated** alarm, with no LPG exposure; `r` requests reset once reset conditions are satisfied.
6. For experimental activity annotation, send `0` (unlabeled), `1` (background), `2` (cooking), or `3` (cleaning) via Serial Monitor.

Detailed bench procedures: [Testing and calibration](docs/TESTING.md) and [Sensor health](docs/SENSOR_HEALTH.md).

### Optional local Wi-Fi dashboard

Copy `firmware/LPG_ENose_ESP32_Kitchen/secrets.example.h` to `secrets.h` in the same folder and set your Wi-Fi credentials, dashboard username and a unique password (at least 12 characters). The actual `secrets.h` is ignored by Git. Open the ESP32's printed LAN IP in a browser.

The local dashboard serves `/`, `/api` and `/csv`; it requires configured credentials. **HTTP Basic authentication is not HTTPS encryption.** Use only a trusted, isolated network, never expose or port-forward the ESP32 to the Internet. See [Dashboard guide](docs/DASHBOARD.md).

### Data collection and offline analysis

For longer USB captures:

```bash
python -m pip install pyserial
python tools/serial_logger.py --port /dev/ttyUSB0 --output captures_day1.csv
```

For offline activity comparisons, collect **at least four independent sessions** with appropriate manual activity labels, then:

```bash
python -m pip install -r ml/requirements.txt
python ml/train.py --inputs day1.csv day2.csv day3.csv day4.csv --output ml/output/report.json
```

The analysis reports session-held-out balanced accuracy, macro-F1 and confusion matrices. Sample CSV content in this repository is **synthetic example data**, not measured or suitable for training. See [Data logging](docs/DATA_LOGGING.md) and [Offline ML methodology](ml/README.md).

## Repository layout

```text
firmware/LPG_ENose_ESP32_Kitchen/  ESP32 C++ firmware, sensor logic, dashboard
docs/                              Wiring, calibration, data and dashboard guides
tools/                             USB serial-to-CSV collection utility
ml/                                Offline features, model comparison, requirements
tests/                             C++ and Python unit tests
data/                              Synthetic CSV format example
.github/workflows/                 Automated build and test checks
```

## Validation and limitations

The project has automated **software compilation and synthetic-data unit tests**, but has **not** established performance on physical hardware under representative LPG exposures. The illustrative signal-rise rules are not certified leak thresholds. Temperature, humidity, cooking vapors, aging, power loss and undetected sensor failures can affect readings. A normal display does not establish that a kitchen is gas-free.

There is **no validated ppm/%LEL output, certified leak sensitivity, measured missed-detection rate, secure remote alerting or automatic gas shutoff**. The system must not actuate electrical fans, mains relays or gas valves in a potentially hazardous atmosphere. Any LPG response experiments belong in a suitably equipped, professionally supervised laboratory.

## Contributions

Issues and focused pull requests are welcome for improving reproducibility, sensor characterization, data quality, and documentation. Distinguish clearly between simulated tests, laboratory measurements and safety-certified capabilities.
