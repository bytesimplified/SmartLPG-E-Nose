# Research CSV collection

The ESP32 records sensor values once a second and emits `CSV,...` lines on its 115200-baud USB serial port. The most recent **300** rows (roughly five minutes at 1 Hz) are also held in RAM and downloadable at `http://<esp32-ip>/csv` when Wi-Fi is configured. The ring buffer is erased on reboot; it is **not** a durable data store.

## Data columns

- `uptime_ms`: ESP32 time since boot, milliseconds; **not a wall-clock timestamp** (wraps at about 49.7 days).
- `mq6_mv`, `mq2_mv`: approximate analog-output voltages, **not ppm or %LEL**.
- `temperature_c`, `humidity_pct`: optional CRC-validated SHT31 readings; blank when unavailable.
- `env_valid`, `alarm`, `fault`: 0/1 indicators of reported device state.
- `status`: status string at time of sample.
- `activity_label`: **manual annotation only**, not verified gas identity.

## Recording longer sessions via USB

Install Python 3 and `pyserial`:

```bash
python -m pip install pyserial
python tools/serial_logger.py --port /dev/ttyUSB0 --output session_001.csv
# Windows example: --port COM3
```

The script creates a **new** file (refuses to overwrite an existing one), adds `host_utc` ISO timestamps from the computer, and flushes each row. Use `--max-rows 600` to stop after 600 valid records, or Ctrl+C.

The optional LAN endpoint and dashboard are not authenticated. Only use them on a trusted isolated network; disable Wi-Fi for offline studies.

## Manual activity annotations (Serial Monitor)

- `0`: unlabeled (default)
- `1`: ordinary background observation
- `2`: cooking activity
- `3`: cleaning activity

These labels are observational context and may contain mixtures or unknown contaminants. There is intentionally **no DIY "confirmed LPG" label**. Valid LPG reference measurements, concentrations, and detection accuracy require qualified controlled-lab testing with appropriate equipment.

## Reproducibility checklist

Record sensor module model/batch, power conditions, firmware commit hash, warm-up and burn-in, calibration time, room/ventilation conditions, SHT31 availability, sampling rate, activity start/end times, and dropped/invalid serial packets. Split evaluation by **session/day** to reduce train/test leakage. Do not treat unlabeled home data as confirmed negative reference gas measurements.

`data/sample_readings.csv` contains **fabricated parsing examples only**, not measured data. Do not train or evaluate a model on that sample.
