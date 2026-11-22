# Wiring and installation notes

> **Educational prototype, not a household LPG alarm.** Use a separately certified and correctly installed LPG detector. The prototype must not drive a valve, exhaust fan, ignition system, or mains appliance.

## Supported hardware
- Classic ESP32 DevKit (ESP32-WROOM-32-style boards, not ESP32-C3/S3)
- MQ-6 and MQ-2 modules with analog outputs
- Regulated 5 V supply sized for both sensor heaters, a USB/3.3 V supply appropriate for the ESP32
- Two independent voltage dividers (four 10 kΩ resistors)
- Current-limited red and green LEDs
- Active buzzer **with a transistor driver** if current exceeds the GPIO limit
- Momentary push button

## Pinout

| Signal | ESP32 |
| --- | --- |
| MQ-6 AO (divided) | GPIO34 (ADC1) |
| MQ-2 AO (divided) | GPIO35 (ADC1) |
| Active buzzer driver | GPIO25 |
| Red status LED (with series resistor) | GPIO26 |
| Green status LED (with series resistor) | GPIO27 |
| Reset button to ground | GPIO33 (internal pull-up) |
| MQ-6/MQ-2 module VCC | Regulated **5 V** per module documentation |
| Every module GND | Common GND |

## Critical ADC protection

**Do not connect a 5 V MQ module's AO pin directly to ESP32 ADC inputs.** Build this exact circuit independently for MQ-6 and MQ-2:

```text
MQ AO ----[10 kΩ]----+---- ESP32 GPIO34 (MQ-6) / GPIO35 (MQ-2)
                     |
                  [10 kΩ]
                     |
                    GND
```

At a 5 V AO signal, the divider produces about 2.5 V at the ESP32 input. Verify actual input voltage with a multimeter before connecting the ESP32. The sketch multiplies the measured ADC-side millivolts by two to estimate the original analog output. That voltage is **not ppm or %LEL**.

Sensor heaters can draw significant current. Do not power them directly from an ESP32 GPIO. Check the power rating and follow the sensor/module datasheets.

## Placement

LPG generally settles toward lower areas because it is heavier than air, but placement is affected by appliance geometry, airflow, obstructions, and applicable standards. Follow the **certified** detector manufacturer's placement and installation instructions. For the experimental sensing board, keep it away from flames, grease, moisture, physical damage, and unprotected electrical equipment. No DIY breadboard system should be treated as intrinsically safe or explosion-proof.

## Actuator safety

This project **does not** switch ventilation, relays, solenoid valves, power outlets, or gas supplies. Do not add such controls to an occupied kitchen without qualified engineering and certified equipment. In a suspected leak, follow your gas provider's emergency guidance; do not operate electrical switches.

## Before powering up

1. Confirm 10 kΩ / 10 kΩ dividers exist on **both** analog channels.
2. Verify a shared ground and regulated sensor supply.
3. Check LEDs include current-limiting resistors and buzzer driver matches load.
4. Make sure the independent certified LPG detector is working.
5. Test the project on a laboratory bench without deliberately releasing LPG.
