#include <cassert>
#include <iostream>

#include "../firmware/LPG_ENose_ESP32_Kitchen/ENoseLogic.h"

int main() {
  const enose::Rules r = {
    1.35f, 180.0f, 1.60f, 300.0f, 1.20f, 120.0f
  };

  // Resting signals and small changes should not trigger the example rule.
  assert(!enose::looksLikeGasRise(1000, 1000, 1000, 1000, r));
  assert(!enose::looksLikeGasRise(1100, 1100, 1000, 1000, r));
  // MQ-6 can trigger without the MQ-2 channel.
  assert(enose::looksLikeGasRise(1400, 1000, 1000, 1000, r));
  // MQ-2 can trigger without MQ-6.
  assert(enose::looksLikeGasRise(1000, 1700, 1000, 1000, r));
  // Joint weaker rises can trigger the combined rule.
  assert(enose::looksLikeGasRise(1220, 1220, 1000, 1000, r));
  // The joint threshold is inclusive at the configured ratio and delta.
  assert(enose::looksLikeGasRise(1200, 1200, 1000, 1000, r));
  assert(!enose::looksLikeGasRise(1199, 1199, 1000, 1000, r));
  // Invalid/nonpositive calibration is not a basis for classifying.
  assert(!enose::looksLikeGasRise(1400, 1400, 0, 1000, r));
  assert(!enose::looksLikeGasRise(1400, 1400, 1000, -1, r));

  std::cout << "Sensor-rise rule unit tests passed (synthetic voltages only).\n";
  return 0;
}
