#pragma once
#include <stdint.h>
#include <cmath>

// SHT31 framing/CRC and conversion; designed to compile on host or ESP32.
namespace enose {

inline uint8_t sht31Crc(const uint8_t* bytes, unsigned n) {
  uint8_t crc = 0xFF;
  for (unsigned i = 0; i < n; ++i) {
    crc ^= bytes[i];
    for (unsigned b = 0; b < 8; ++b)
      crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 0x31) : uint8_t(crc << 1);
  }
  return crc;
}

struct Environment {
  float temperatureC = NAN;
  float humidityPct = NAN;
  bool valid = false;
};

inline Environment parseSht31(const uint8_t* d, unsigned n) {
  Environment out;
  if (!d || n != 6 || sht31Crc(d, 2) != d[2] || sht31Crc(d + 3, 2) != d[5])
    return out;
  const uint16_t t = (uint16_t(d[0]) << 8) | d[1];
  const uint16_t h = (uint16_t(d[3]) << 8) | d[4];
  out.temperatureC = -45.0f + 175.0f * float(t) / 65535.0f;
  out.humidityPct = 100.0f * float(h) / 65535.0f;
  out.valid = std::isfinite(out.temperatureC) && std::isfinite(out.humidityPct) &&
              out.temperatureC >= -40.0f && out.temperatureC <= 125.0f &&
              out.humidityPct >= 0.0f && out.humidityPct <= 100.0f;
  return out;
}
} // namespace enose
