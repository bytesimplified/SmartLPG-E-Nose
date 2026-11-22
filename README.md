# SmartLPG-E-Nose 👃

### Can software *smell* a gas leak?

A human walks into a kitchen, notices a suspicious smell, and thinks **"Is the LPG leaking?"**

But how do you teach a microcontroller to notice the same thing? A computer has no nose. It needs a **sensor that converts something happening in the physical world into data** — and then software that makes sense of that data.

That's what **SmartLPG-E-Nose** explores: an ESP32-based electronic-nose research project for understanding gas-responsive signals, kitchen interference, and what multiple inexpensive sensors can tell us together.

> [!WARNING]
> **This is an experimental learning/research platform, NOT a certified LPG alarm.** Use a separately certified household LPG detector. Do not deliberately release LPG in a kitchen to test this project.

## First, what does it mean to "detect" LPG?

LPG is mainly propane and butane, which are naturally almost odorless. The recognizable "gas smell" comes from added odorants such as mercaptans. **Humans mainly notice the odorant, not the fuel molecules themselves.**

There are several different ways to turn a possible leak into a measurable signal:

| Approach | What actually happens? | CS way to think about it |
| --- | --- | --- |
| **Human nose** 👃 | Odorant molecules activate olfactory receptors; the brain recognizes a smell pattern. | A biological **sensor array + pattern recognizer**. |
| **Semiconductor / MOS sensor** | A heated metal-oxide material (e.g. SnO₂ in MQ sensors) changes electrical resistance when exposed to certain gases. | An analog **feature generator**: gas exposure → changing numeric signal. |
| **Catalytic bead / pellistor** 🔥 | A heated catalyst promotes oxidation of combustible gas. The **extra reaction heat** changes the resistance of a sensing element relative to a reference. | **Chemical reaction → temperature difference → electrical signal**. Not just an ambient thermistor. |
| **Infrared (IR/NDIR)** | Some hydrocarbon molecules absorb particular infrared wavelengths; an optical sensor measures the change. | A wavelength-specific **optical measurement**. |
| **DNA-functionalized sensors** 🧬 | Selected single-stranded DNA coatings on graphene or nanotubes can change how different vapor molecules interact with a sensor, changing its electrical response. | Think **customizable input features** for a molecular sensor array. This is experimental research, **not LPG containing DNA or a validated LPG DNA sequence**. |
| **Ultrasonic leak detection** | A pressurized leak can generate high-frequency acoustic noise. This detects leak-related sound rather than measuring gas concentration. | A **signal-processing problem**, useful in some industrial settings. |

So **gas sensing isn't one algorithm or one chemistry**. Different physical mechanisms generate different signals, with different limitations. And none of these should be DIY-engineered into the sole safety device for a home.

## The idea we're building: an electronic nose

Imagine developing a spam classifier using *one keyword*. It might catch obvious spam, but you'll quickly hit false positives and false negatives.

Now imagine giving it several signals: text patterns, sender reputation, links, context. You can learn much more — although you still need trustworthy labels and validation.

**An E-Nose has a similar motivation.** Instead of trusting one gas sensor's threshold, it observes *different sensor responses* and the environment around them.

```text
Physical world                      Embedded system                  Data / software
─────────────────────────────────────────────────────────────────────────────────────────
MQ-6 (LPG-responsive MOS) ── ADC ──┐
                                   │     ┌───────────────┐      ┌─────────────────────────┐
MQ-2 (broad gas MOS) ────── ADC ───┼────▶│     ESP32     │─────▶│ Local buzzer + LEDs     │
                                   │     │ sampling      │      │ Serial CSV / /api /csv  │
SHT31 (temp + humidity) ── I²C ────┘     │ diagnostics   │      │ Live browser dashboard  │
                                         └───────────────┘      └────────────┬────────────┘
                                                                            │
                                                                            ▼
                                                              Python offline feature extraction
                                                              + session-aware model comparison
```

We're currently using **MQ-6 + MQ-2**, not a DNA sensor or a catalytic-bead module. The SHT31 is optional and gives temperature/humidity context — it does **not** chemically detect LPG.

### Translate it into a CS architecture

| Concept you already know | In this project |
| --- | --- |
| **Data sources / device drivers** | MQ-2 and MQ-6 produce analog values; SHT31 provides digital temperature/humidity readings. |
| **Ingestion + preprocessing** | ESP32 samples the signals, averages readings and compares with a manually captured clean-air baseline. |
| **Rules engine** | Illustrative relative-rise rules drive a **prototype** local warning; they are not calibrated LPG leak thresholds. |
| **Health checks / observability** | Heuristics flag low, saturated or suspiciously flat sensor values. A passing check doesn't guarantee a healthy sensor. |
| **Event stream + logging** | 1 Hz CSV records, a short 300-sample RAM ring buffer, USB serial logging with host UTC timestamps. |
| **Feature engineering** | Raw signals, one-step changes, rolling averages, MQ-2 vs MQ-6 difference, optional environmental values. |
| **Offline ML experiments** | Random Forest comparisons using independently recorded sessions, **for manually labeled background/cooking/cleaning activities**. |
| **Monitoring UI** | ESP32-served LAN dashboard with live plots, status, fault information and CSV download. |

**Important:** The offline ML models **do not classify verified LPG leaks**, and they are **not connected to the alarm decision**. No amount of software accuracy on everyday activity labels proves that a system will detect a real leak.

## What makes this interesting?

The real challenge isn't making a buzzer beep when an ADC value crosses a number. It's asking good questions about noisy, imperfect data:

- **Cross-sensitivity:** Will cooking fumes, cleaning products or smoke produce similar sensor patterns?
- **Environment:** How much do humidity and temperature influence the readings?
- **Drift:** Does a baseline captured today still describe the sensor a week later?
- **Failure modes:** What happens when a sensor is disconnected, stuck or saturated?
- **Evaluation:** Can a model generalize to a **new recording session/day**, instead of memorizing consecutive samples from the same experiment?

This repo provides the instrumentation and offline analysis scaffolding for exploring those questions. **No validated LPG accuracy, false-negative rate, ppm or %LEL values are claimed.**

## Hardware: what to connect

Use a **classic ESP32 DevKit**, MQ-6, MQ-2, an optional SHT31, red/green LEDs, a buzzer with a suitable driver, and a reset push button.

| Component | ESP32 connection |
| --- | --- |
| MQ-6 analog output | **GPIO34**, through its own 10 kΩ / 10 kΩ divider |
| MQ-2 analog output | **GPIO35**, through a *separate* 10 kΩ / 10 kΩ divider |
| SHT31 (optional) | SDA **GPIO21**, SCL **GPIO22**, 3.3 V-compatible I²C |
| Buzzer driver / red LED / green LED | **GPIO25 / GPIO26 / GPIO27** |
| Reset button | **GPIO33** to ground |

MQ modules often run from a 5 V supply and their analog outputs may exceed ESP32 input limits. **Never wire their AO pins directly to the 3.3 V ESP32 ADC pins.** Use the correct power supply, shared ground, signal dividers and LED current-limiting resistors. Follow [the full wiring guide](docs/WIRING.md) and [SHT31 guide](docs/ENVIRONMENT.md).

## Run the prototype

1. Open [the Arduino firmware](firmware/LPG_ENose_ESP32_Kitchen/LPG_ENose_ESP32_Kitchen.ino) in Arduino IDE with a compatible classic ESP32 board selected.
2. Verify the bench wiring; complete the MQ sensors' manufacturer-required initial conditioning. The firmware's **3-minute restart warm-up is not a substitute for initial burn-in**.
3. Open Serial Monitor at **115200 baud**.
4. In **verified clean air**, enter `c` to collect a baseline. Enter `t` to simulate an alarm **without releasing gas**, and `r` to request reset after the clear interval.
5. For normal activity annotations: `0` = unlabeled, `1` = background, `2` = cooking, `3` = cleaning.

For an optional LAN dashboard, copy `secrets.example.h` to ignored `secrets.h` in the same firmware folder, set Wi-Fi and dashboard credentials, and open the ESP32's local IP address. See [dashboard setup](docs/DASHBOARD.md). **HTTP Basic authentication is not HTTPS**: use an isolated trusted LAN only, never port-forward the device.

### Collect data and experiment offline

```bash
python -m pip install pyserial
python tools/serial_logger.py --port /dev/ttyUSB0 --output day1.csv

python -m pip install -r ml/requirements.txt
python ml/train.py --inputs day1.csv day2.csv day3.csv day4.csv --output ml/output/report.json
```

The model comparison requires multiple **independent real recording sessions** with suitable activity annotations. [The bundled CSV example](data/sample_readings.csv) is **synthetic and only demonstrates formatting** — do not train on it. Read [the data collection guide](docs/DATA_LOGGING.md) and [ML methodology](ml/README.md).

## What's in the repo?

```text
firmware/LPG_ENose_ESP32_Kitchen/   Arduino C++, sensor diagnostics, local dashboard
docs/                               Wiring, calibration, testing, logging, dashboard
tools/                              USB serial → CSV collector
ml/                                 Offline feature extraction + model comparisons
tests/                              Synthetic-input C++ and Python tests
data/                               Example CSV format (not real measurements)
.github/workflows/                  ESP32 build and automated test checks
```

## Reality check

The code compiles and the software tests use synthetic inputs; **physical LPG detection performance has not been validated**. Metal-oxide sensors can respond to unrelated gases, drift with environment, or fail without being recognized. The readings are **estimated sensor output millivolts**, not concentrations or certified safe/unsafe thresholds.

This system is **not a replacement for a certified household LPG alarm**. It has no certified fail-safe design, verified missed-leak rate, remotely secure alarm or automatic shutoff. Never use a DIY board to switch fans, mains appliances or gas valves during a suspected leak. Actual LPG exposure testing requires qualified personnel and an appropriately equipped controlled laboratory.

### Where it could go next

Explore better signal features, multi-day drift analysis, cross-sensitivity studies, properly validated labeled datasets, and — separately — research-grade catalytic, optical or DNA-functionalized sensing modules. **The E-Nose architecture is extensible; the physics still has to be proven.**
