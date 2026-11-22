# SHT31 environmental sensing

This PR adds **temperature (°C)** and **relative humidity (%)** from an optional SHT31 I2C module. It does **not** yet compensate or override MQ alarm decisions, which remains an important safety boundary for experimental models.

## Connection (classic ESP32)
- SHT31 module **SDA → GPIO21**, **SCL → GPIO22**, **GND → ESP32 GND**
- Use **3.3 V** for a 3.3 V compatible SHT31 breakout. Confirm its I2C pull-ups also go to 3.3 V and that the module does not drive I2C pins to 5 V.
- Default I2C address **0x44**. Change `SHT31_ADDRESS` if your board is configured for 0x45.

## Readout

The firmware requests a high-repeatability SHT31 single-shot measurement once every 5 seconds (command 0x2400). It validates the two CRC-8 checksums and reports unavailable values as JSON `null`, plus `env_valid=false`, if no sensor is connected or the data are invalid. The dashboard shows either values or 'unavailable'.

Temperature/humidity **are metadata** for offline analysis of environmental effects. These values **do not make an MQ sensor LPG-selective**. An invalid or absent SHT31 does not disable the existing MQ monitoring or independent local warning.

## Tests
Synthetic SHT31 frames are tested on the host (correct CRC, conversion around 25 °C / 50% RH, bad CRC and incomplete frame). The ESP32 build is checked by GitHub Actions. No physical humidity or gas exposure tests are included.
