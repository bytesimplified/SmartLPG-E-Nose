#pragma once

// Pure C++11 classifier for host-side unit tests.
// Values are analog-output millivolts, NOT LPG concentration or %LEL.
namespace enose {

struct Rules {
  float mq6StrongRatio;
  float mq6StrongDeltaMv;
  float mq2StrongRatio;
  float mq2StrongDeltaMv;
  float bothRatio;
  float bothDeltaMv;
};

inline bool looksLikeGasRise(float mq6mV, float mq2mV,
                             float base6mV, float base2mV,
                             const Rules& r) {
  // The caller also checks ADC faults, warming and calibration.
  // Require valid positive baselines; invalid/NaN values fail comparisons.
  if (!(base6mV > 0.0f && base2mV > 0.0f)) return false;

  const float delta6 = mq6mV - base6mV;
  const float delta2 = mq2mV - base2mV;

  const bool mq6Strong =
      mq6mV >= base6mV * r.mq6StrongRatio &&
      delta6 >= r.mq6StrongDeltaMv;
  const bool mq2Strong =
      mq2mV >= base2mV * r.mq2StrongRatio &&
      delta2 >= r.mq2StrongDeltaMv;
  const bool bothRising =
      mq6mV >= base6mV * r.bothRatio &&
      mq2mV >= base2mV * r.bothRatio &&
      delta6 >= r.bothDeltaMv &&
      delta2 >= r.bothDeltaMv;

  // This is an illustrative voltage-rise rule, NOT certified leak detection.
  return mq6Strong || mq2Strong || bothRising;
}

}  // namespace enose
