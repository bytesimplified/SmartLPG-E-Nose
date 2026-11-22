#!/usr/bin/env python3
"""Save CSV-prefixed ESP32 readings with host UTC timestamps.

Not a calibrated LPG or %LEL measurement. Do not use this for safety alarms.
"""
import argparse
import csv
from datetime import datetime, timezone
from pathlib import Path

FIELDS = ["uptime_ms", "mq6_mv", "mq2_mv", "temperature_c", "humidity_pct",
          "env_valid", "alarm", "fault", "status", "activity_label"]
HEADER = ["host_utc"] + FIELDS


def parse_wire_line(line):
    """Return validated CSV values, or None for diagnostics/header/noise."""
    if not line.startswith("CSV,"):
        return None
    try:
        cells = next(csv.reader([line.strip()]))
        if len(cells) != len(FIELDS) + 1 or cells[0] != "CSV":
            return None
        values = cells[1:]
        int(values[0])
        float(values[1])
        float(values[2])
        for v in values[3:5]:
            if v:
                float(v)
        if any(v not in ("0", "1") for v in values[5:8]):
            return None
        if values[9] not in ("unlabeled", "background", "cooking", "cleaning"):
            return None
        return values
    except (ValueError, csv.Error):
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Example: /dev/ttyUSB0 or COM3")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--output", type=Path, default=Path("enose_capture.csv"))
    parser.add_argument("--max-rows", type=int, default=0,
                        help="0 means collect until Ctrl+C")
    args = parser.parse_args()
    try:
        import serial  # pip install pyserial
    except ImportError as exc:
        parser.error("Install pyserial first: python -m pip install pyserial")

    count = 0
    # Exclusive creation protects against accidental data loss.
    with args.output.open("x", newline="", encoding="utf-8") as out:
        writer = csv.writer(out)
        writer.writerow(HEADER)
        with serial.Serial(args.port, args.baud, timeout=2) as device:
            try:
                while args.max_rows <= 0 or count < args.max_rows:
                    raw = device.readline().decode("utf-8", errors="replace")
                    parsed = parse_wire_line(raw)
                    if parsed is None:
                        continue
                    writer.writerow([datetime.now(timezone.utc).isoformat()] + parsed)
                    out.flush()
                    count += 1
            except KeyboardInterrupt:
                pass
    print(f"Saved {count} readings to {args.output}")


if __name__ == "__main__":
    main()
