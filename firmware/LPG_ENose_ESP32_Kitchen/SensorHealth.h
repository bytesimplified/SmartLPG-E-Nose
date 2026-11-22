#pragma once
#include <cmath>
#include <stdint.h>

// Diagnostic heuristics only. Sensor health cannot be proven from AO voltage.
// All signal values are estimated sensor analog-output millivolts, NOT ppm.
namespace enose {

enum class SensorFault : uint8_t {
  None = 0,
  DisconnectedOrLow,
  Saturated,
  Flatline
};

inline const char* sensorFaultName(SensorFault fault) {
  switch (fault) {
    case SensorFault::DisconnectedOrLow: return "LOW OR DISCONNECTED SIGNAL";
    case SensorFault::Saturated: return "SATURATED SIGNAL";
    case SensorFault::Flatline: return "POSSIBLE STUCK SIGNAL";
    default: return "NONE";
  }
}

class SensorHealthMonitor {
 public:
  // 1 Hz samples => 15 minutes of precisely unchanged ADC values.
  // This is a suspicion flag, not a proof of sensor malfunction.
  SensorHealthMonitor()
      : invalidCount_(0), saturatedCount_(0), flatlineCount_(0),
        havePrevious_(false), last6_(0), last2_(0) {}

  SensorFault update(float mq6, float mq2) {
    bool invalid = !std::isfinite(mq6) || !std::isfinite(mq2) ||
                   mq6 < 30.0f || mq2 < 30.0f;
    bool saturated = !invalid && (mq6 >= 4800.0f || mq2 >= 4800.0f);
    invalidCount_ = invalid ? bump(invalidCount_, 5) : 0;
    saturatedCount_ = saturated ? bump(saturatedCount_, 5) : 0;
    bool same = havePrevious_ && !invalid && mq6 == last6_ && mq2 == last2_;
    flatlineCount_ = same ? bump(flatlineCount_, 900) : 0;
    last6_ = mq6;
    last2_ = mq2;
    havePrevious_ = true;
    if (invalidCount_ >= 5) return SensorFault::DisconnectedOrLow;
    if (saturatedCount_ >= 5) return SensorFault::Saturated;
    if (flatlineCount_ >= 900) return SensorFault::Flatline;
    return SensorFault::None;
  }

 private:
  static int bump(int n, int max) { return n < max ? n + 1 : max; }
  int invalidCount_, saturatedCount_, flatlineCount_;
  bool havePrevious_;
  float last6_, last2_;
};
}  // namespace enose
