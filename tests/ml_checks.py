import unittest
import pandas as pd

from ml.features import build_features, SINGLE, DUAL, ENV


class FeatureTests(unittest.TestCase):
    def test_session_differences_do_not_cross_boundaries(self):
        rows = [
            dict(session_id="day1", uptime_ms=0, mq6_mv=100, mq2_mv=90,
                 temperature_c=23, humidity_pct=50, env_valid=1, activity_label="background"),
            dict(session_id="day1", uptime_ms=1000, mq6_mv=130, mq2_mv=100,
                 temperature_c=24, humidity_pct=51, env_valid=1, activity_label="cooking"),
            dict(session_id="day2", uptime_ms=0, mq6_mv=500, mq2_mv=450,
                 temperature_c=0, humidity_pct=0, env_valid=0, activity_label="background"),
        ]
        df = build_features(pd.DataFrame(rows))
        self.assertEqual(df["mq6_delta"].tolist(), [0, 30, 0])
        self.assertEqual(df["mq2_delta"].tolist(), [0, 10, 0])
        self.assertEqual(df["mq6_rolling5"].tolist(), [100, 115, 500])
        self.assertTrue(pd.isna(df.iloc[2]["temperature_c"]))
        self.assertTrue(pd.isna(df.iloc[2]["humidity_pct"]))
        self.assertTrue(set(SINGLE).issubset(DUAL))
        self.assertTrue(set(DUAL).issubset(ENV))

    def test_missing_columns_rejected(self):
        with self.assertRaises(ValueError):
            build_features(pd.DataFrame({"mq6_mv": [1]}))


if __name__ == "__main__":
    unittest.main()
