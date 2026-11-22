# Offline E-Nose research pipeline

**Purpose:** Compare how well MQ-6 alone, MQ-6+MQ-2, and MQ-6+MQ-2+temperature/humidity distinguish **manually annotated normal activities** (background, cooking, cleaning). This is **not an LPG leak classifier or a certified safety component**.

## Installation

```bash
python -m venv .venv
# Activate the virtual environment for your operating system.
python -m pip install -r ml/requirements.txt
```

First, record independent serial sessions following [data logging](../docs/DATA_LOGGING.md). Record genuine background, cooking and cleaning activity without intentionally releasing LPG indoors; use the existing independent certified detector for safety.

```bash
python ml/train.py --inputs captures/day1.csv captures/day2.csv captures/day3.csv captures/day4.csv --output ml/output/activity-report.json
```

## Reproducible feature comparisons

- **MQ-6 only:** raw mV, one-step voltage difference, causal five-sample rolling mean
- **Both MQ sensors:** same three features for MQ-2 plus voltage gap
- **With environment:** both MQ sensors plus SHT31 temperature, relative humidity and validity flag
- **Majority-class baseline:** checks whether learning outperforms trivial label imbalance

Each CSV file is treated as a separate session; entire sessions are held out so a model cannot memorize adjacent samples from the same cooking episode. Numeric gaps are filled using training-only medians. A deterministic Random Forest is trained and class-wise metrics are written to JSON.

The program requires at least four independent sessions and two activity classes present in more than one session. It **rejects** the synthetic sample dataset. You should still expand the dataset to weeks/days across different kitchens and record environmental conditions before drawing any conclusion.

## Interpretation

Classification metrics here are only for **normal activity labels**, not LPG sensitivity, %LEL response, missed-leak probability or false-alarm safety performance. Even excellent scores cannot justify changing the certified gas alarm or remotely triggering an LPG shutoff. Experiments involving known LPG concentrations require an equipped supervised lab and professionally calibrated reference instrumentation.

A future research phase could evaluate independently acquired, validated gas-exposure data under appropriate supervision, using proper session-level splits and reporting false-negative performance.
