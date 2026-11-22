#pragma once
#include <stdint.h>

// Declared in a header because the Arduino .ino preprocessor automatically
// generates function prototypes before source-level struct declarations.
struct Observation {
  uint32_t uptimeMs;
  float mq6mV, mq2mV, temperatureC, humidityPct;
  bool envValid, alarm, fault;
  const char* status;
  char label;
};
