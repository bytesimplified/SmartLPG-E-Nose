"""Session-aware E-Nose features for *offline* activity analysis.

These features describe uncalibrated sensor voltage. They do not estimate
LPG ppm/%LEL and must not be used to override a certified detector.
"""
import pandas as pd

REQUIRED = ("session_id", "uptime_ms", "mq6_mv", "mq2_mv",
            "temperature_c", "humidity_pct", "env_valid", "activity_label")
SINGLE = ["mq6_mv", "mq6_delta", "mq6_rolling5"]
DUAL = SINGLE + ["mq2_mv", "mq2_delta", "mq2_rolling5", "voltage_gap"]
ENV = DUAL + ["temperature_c", "humidity_pct", "env_valid"]


def build_features(data):
    missing = set(REQUIRED) - set(data.columns)
    if missing:
        raise ValueError(f"Missing columns: {sorted(missing)}")
    df = data.copy()
    for col in ("uptime_ms", "mq6_mv", "mq2_mv",
                "temperature_c", "humidity_pct", "env_valid"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df = df.dropna(subset=["uptime_ms", "mq6_mv", "mq2_mv", "session_id"])
    df = df.sort_values(["session_id", "uptime_ms"], kind="stable").reset_index(drop=True)
    grouped = df.groupby("session_id", sort=False)
    for name in ("mq6", "mq2"):
        col = f"{name}_mv"
        df[f"{name}_delta"] = grouped[col].diff().fillna(0.0)
        df[f"{name}_rolling5"] = grouped[col].transform(
            lambda series: series.rolling(5, min_periods=1).mean())
    df["voltage_gap"] = df["mq6_mv"] - df["mq2_mv"]
    # An invalid SHT31 reading must never be interpreted as a true 0 °C / 0% RH.
    df.loc[df["env_valid"] != 1, ["temperature_c", "humidity_pct"]] = float("nan")
    df["env_valid"] = df["env_valid"].fillna(0).astype(int)
    return df
