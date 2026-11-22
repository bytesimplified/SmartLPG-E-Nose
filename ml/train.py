#!/usr/bin/env python3
"""Compare E-Nose signals in OFFLINE activity classification only.

Does not classify verified LPG leaks, estimate ppm/%LEL, or control alarms.
"""
import argparse
import json
from pathlib import Path

import pandas as pd
from sklearn.dummy import DummyClassifier
from sklearn.ensemble import RandomForestClassifier
from sklearn.impute import SimpleImputer
from sklearn.metrics import balanced_accuracy_score, classification_report, confusion_matrix, f1_score
from sklearn.model_selection import GroupShuffleSplit
from sklearn.pipeline import Pipeline

from features import build_features, SINGLE, DUAL, ENV

ACTIVITIES = {"background", "cooking", "cleaning"}


def load_sessions(paths):
    frames = []
    for index, path in enumerate(paths):
        if path.name == "sample_readings.csv":
            raise ValueError("The synthetic sample is for parser tests, not model training")
        df = pd.read_csv(path)
        df["session_id"] = f"capture_{index}_{path.stem}"
        frames.append(df)
    if not frames:
        raise ValueError("Provide multiple independently captured CSV sessions")
    merged = build_features(pd.concat(frames, ignore_index=True))
    merged = merged[merged["activity_label"].isin(ACTIVITIES)].copy()
    if len(merged) < 40 or merged["session_id"].nunique() < 4:
        raise ValueError("Collect >=40 labeled observations across >=4 independent sessions")
    if merged["activity_label"].nunique() < 2:
        raise ValueError("Need at least two distinct activity classes")
    for label, df in merged.groupby("activity_label"):
        if df["session_id"].nunique() < 2:
            raise ValueError(f"Label {label!r} appears in fewer than two sessions")
    return merged


def find_split(df):
    labels = set(df["activity_label"])
    splitter = GroupShuffleSplit(n_splits=100, test_size=0.25, random_state=42)
    for train_idx, test_idx in splitter.split(
            df, df["activity_label"], groups=df["session_id"]):
        if (set(df.iloc[train_idx]["activity_label"]) == labels and
                set(df.iloc[test_idx]["activity_label"]) == labels):
            return train_idx, test_idx
    raise ValueError("Cannot make a session-disjoint train/test split with all labels; collect more sessions")


def compare_models(df):
    tr, te = find_split(df)
    train, test = df.iloc[tr], df.iloc[te]
    classes = sorted(df["activity_label"].unique())
    model_columns = {"mq6_only": SINGLE, "mq2_mq6": DUAL,
                     "mq2_mq6_environment": ENV}
    report = {
        "purpose": "Offline exploratory classification of manually annotated activities; NOT LPG leak detection",
        "n_rows": int(len(df)),
        "n_sessions": int(df["session_id"].nunique()),
        "train_sessions": sorted(train["session_id"].unique().tolist()),
        "test_sessions": sorted(test["session_id"].unique().tolist()),
        "classes": classes,
        "models": {}
    }
    ytr, yte = train["activity_label"], test["activity_label"]
    for name, columns in model_columns.items():
        pipeline = Pipeline([
            ("impute", SimpleImputer(strategy="median", keep_empty_features=True)),
            ("classifier", RandomForestClassifier(
                n_estimators=120, class_weight="balanced", random_state=42))
        ])
        pipeline.fit(train[columns], ytr)
        pred = pipeline.predict(test[columns])
        report["models"][name] = {
            "features": columns,
            "balanced_accuracy": float(balanced_accuracy_score(yte, pred)),
            "macro_f1": float(f1_score(yte, pred, average="macro", zero_division=0)),
            "confusion_matrix": confusion_matrix(yte, pred, labels=classes).tolist(),
            "classification_report": classification_report(
                yte, pred, labels=classes, output_dict=True, zero_division=0)
        }
    baseline = DummyClassifier(strategy="most_frequent")
    baseline.fit(train[SINGLE], ytr)
    yhat = baseline.predict(test[SINGLE])
    report["baseline_majority"] = {
        "balanced_accuracy": float(balanced_accuracy_score(yte, yhat)),
        "macro_f1": float(f1_score(yte, yhat, average="macro", zero_division=0))
    }
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, nargs="+", required=True,
                        help="Independent serial capture CSV files, one per session")
    parser.add_argument("--output", type=Path, default=Path("ml/output/report.json"))
    args = parser.parse_args()
    report = compare_models(load_sessions(args.inputs))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"Offline activity comparison saved to {args.output}")
    for name, metric in report["models"].items():
        print(f"{name}: balanced accuracy={metric['balanced_accuracy']:.3f}; "
              f"macro F1={metric['macro_f1']:.3f}")
    print("These scores do not measure LPG detection accuracy.")


if __name__ == "__main__":
    main()
