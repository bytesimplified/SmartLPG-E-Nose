#include <cassert>
#include <iostream>

#include "../firmware/LPG_ENose_ESP32_Kitchen/ENoseLogic.h"
#include "../firmware/LPG_ENose_ESP32_Kitchen/SensorHealth.h"
#include "../firmware/LPG_ENose_ESP32_Kitchen/Environment.h"

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

  enose::SensorHealthMonitor low;
  for (int i = 0; i < 4; ++i) assert(low.update(0, 1000) == enose::SensorFault::None);
  assert(low.update(0, 1000) == enose::SensorFault::DisconnectedOrLow);
  assert(low.update(1001, 1002) == enose::SensorFault::None);

  enose::SensorHealthMonitor high;
  for (int i = 0; i < 4; ++i) assert(high.update(4900, 1000) == enose::SensorFault::None);
  assert(high.update(4900, 1000) == enose::SensorFault::Saturated);

  enose::SensorHealthMonitor stuck;
  assert(stuck.update(1000, 1000) == enose::SensorFault::None);
  for (int i = 0; i < 899; ++i)
    assert(stuck.update(1000, 1000) == enose::SensorFault::None);
  assert(stuck.update(1000, 1000) == enose::SensorFault::Flatline);
  assert(stuck.update(1000, 1001) == enose::SensorFault::None);

  std::cout << "Sensor-rise and sensor-health tests passed (synthetic values only).\\n";
  // SHT31 is not an MQ gas sensor; CRC protects I2C reading integrity.
  uint8_t raw[6] = {0x66, 0x66, 0, 0x80, 0x00, 0};
  raw[2] = enose::sht31Crc(raw, 2);
  raw[5] = enose::sht31Crc(raw + 3, 2);
  enose::Environment env = enose::parseSht31(raw, 6);
  assert(env.valid);
  assert(env.temperatureC > 24.9f && env.temperatureC < 25.1f);
  assert(env.humidityPct > 49.9f && env.humidityPct < 50.1f);
  raw[2] ^= 0x01;
  assert(!enose::parseSht31(raw, 6).valid);
  assert(!enose::parseSht31(raw, 5).valid);
  return 0;
}
