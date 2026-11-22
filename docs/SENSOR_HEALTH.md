# Sensor health diagnostics (prototype)

The ESP32 now records diagnostic warnings for either MQ module when the estimated analog output is near zero for 5 consecutive samples, near 4.8 V for 5 samples, or when **both** measured voltages remain *bit-for-bit identical* for 900 consecutive samples (about 15 minutes at 1 Hz). The first two checks are signal plausibility heuristics; the third is a weak indication of a potentially stuck ADC path.

The JSON `/api` includes `fault` and `fault_reason`. A fault disables the green monitoring indicator, flashes the red LED/buzzer and preserves the latched warning if a baseline is available. After inspection and readings returning to normal, the existing manual reset conditions apply.

**Do not treat a green status as proof that there is no LPG.** A normal-looking ADC voltage cannot prove sensor integrity; stuck-high signals, aged/poisoned sensors, heater issues, wiring errors, and undetected failures can still exist. Long periods of stable indoor air may also produce repeated ADC values and cause a nuisance diagnostic.

These checks are *not* safety-certified functional diagnostics and cannot substitute for a certified LPG detector. No gas-response sensitivity or ppm/%LEL validation is implied. Use electronic signal simulation for tests, not controlled LPG releases in a kitchen.
