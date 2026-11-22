/*
  Smart LPG E-Nose - ESP32 Arduino IDE prototype
  Hardware: Classic ESP32 DevKit, MQ-6 AO -> GPIO34,
            MQ-2 AO -> GPIO35 (BOTH through 10k/10k dividers),
            active buzzer driver -> GPIO25, red LED -> GPIO26,
            green LED -> GPIO27, reset button -> GPIO33 to GND.

  This is a STUDENT PROTOTYPE, NOT a certified gas alarm.
  Keep an independent certified household LPG detector installed.
  NEVER attempt leak tests by releasing LPG in a kitchen.
  No ppm/LEL measurement or certified safety threshold is provided.

  Sensor AO -> 10k -> ESP32 ADC input -> 10k -> GND
  Both MQ module VCC pins -> regulated 5V; share GND with ESP32.

  Calibrate deliberately in verified clean air via Serial Monitor 'c'
  (after restart warm-up, and after the sensor's initial burn-in).
  The baseline is stored in ESP32 flash and NOT silently recalibrated.
  Press reset button after the alarm condition has cleared for 30s.
  Serial 't' triggers a simulated alarm WITHOUT using gas.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <math.h>
#include <cstring>
#include "ENoseLogic.h"
#include "SensorHealth.h"
#include "Environment.h"
#include "DashboardPage.h"
#include <Wire.h>

// Optional local secrets.h is ignored by git. No Wi-Fi without explicit setup.
#if defined(__has_include)
#  if __has_include("secrets.h")
#    include "secrets.h"
#  endif
#endif
#ifndef ENOSE_WIFI_SSID
#  define ENOSE_WIFI_SSID ""
#endif
#ifndef ENOSE_WIFI_PASSWORD
#  define ENOSE_WIFI_PASSWORD ""
#endif
#ifndef ENOSE_DASHBOARD_USER
#  define ENOSE_DASHBOARD_USER ""
#endif
#ifndef ENOSE_DASHBOARD_PASSWORD
#  define ENOSE_DASHBOARD_PASSWORD ""
#endif
const char* WIFI_SSID = ENOSE_WIFI_SSID;
const char* WIFI_PASSWORD = ENOSE_WIFI_PASSWORD;
const char* DASHBOARD_USER = ENOSE_DASHBOARD_USER;
const char* DASHBOARD_PASSWORD = ENOSE_DASHBOARD_PASSWORD;

// ---- Pin mapping (classic ESP32, NOT ESP32-C3/S3) ----
constexpr uint8_t PIN_MQ6    = 34;
constexpr uint8_t PIN_MQ2    = 35;
constexpr uint8_t PIN_BUZZER = 25;
constexpr uint8_t PIN_RED    = 26;
constexpr uint8_t PIN_GREEN  = 27;
constexpr uint8_t PIN_RESET  = 33;
constexpr uint8_t SHT31_ADDRESS = 0x44;
constexpr uint32_t ENV_INTERVAL_MS = 5000;

// A 10k series resistor + 10k to GND means the ADC sees half AO.
constexpr float AO_TO_ADC_RATIO = 2.0f;

// ---- Timing ----
constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t RESTART_WARMUP_MS = 180000;   // 3 min, NOT initial sensor burn-in
constexpr uint32_t SAFE_TO_RESET_MS = 30000;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
constexpr int CALIBRATION_SAMPLES = 60;          // 60 x 1 sec = ~60 sec
constexpr int ALARM_CONFIRM_SAMPLES = 3;
constexpr size_t LOG_CAPACITY = 300; // ~5 minutes at 1 Hz in RAM, NOT persistent storage

// ---- EXAMPLE RELATIVE THRESHOLDS; NOT calibrated for ppm or LEL ----
constexpr float MQ6_STRONG_RATIO = 1.35f;
constexpr float MQ6_STRONG_DELTA_MV = 180.0f;
constexpr float MQ2_STRONG_RATIO = 1.60f;
constexpr float MQ2_STRONG_DELTA_MV = 300.0f;
constexpr float BOTH_RATIO = 1.20f;
constexpr float BOTH_DELTA_MV = 120.0f;

// Very low analog input may indicate wiring/power failure, but
// this simple heuristic cannot detect every possible sensor fault.
constexpr float MIN_VALID_SENSOR_MV = 30.0f;

WebServer server(80);
Preferences prefs;

uint32_t bootAt = 0;
uint32_t lastSampleAt = 0;
uint32_t lastEnvAt = 0;
uint32_t safeSince = 0;
uint32_t buttonRawChangedAt = 0;

enose::Environment environment;
bool sht31Present = false;

float mq6mV = 0.0f;                   // Approximate voltage at MQ-6 AO
float mq2mV = 0.0f;                   // Approximate voltage at MQ-2 AO
float baseline6mV = NAN;
float baseline2mV = NAN;

bool haveBaseline = false;
bool calibrating = false;
bool alarmLatched = false;
bool sensorFault = false;
bool webStarted = false;

int calibrationCount = 0;
float calibrationSum6 = 0.0f;
float calibrationSum2 = 0.0f;
int confirmCount = 0;
enose::SensorHealthMonitor healthMonitor;
enose::SensorFault activeFault = enose::SensorFault::None;

bool previousButtonRaw = HIGH;
bool debouncedButton = HIGH;

// Experiment annotations describe activities, not verified gas identities.
char experimentLabel = 'u'; // u=unlabeled, b=background, c=cooking, x=cleaning
const char* labelText(char c) {
  switch (c) {
    case 'b': return "background";
    case 'c': return "cooking";
    case 'x': return "cleaning";
    default: return "unlabeled";
  }
}

struct Observation {
  uint32_t uptimeMs;
  float mq6mV, mq2mV, temperatureC, humidityPct;
  bool envValid, alarm, fault;
  const char* status;
  char label;
};
Observation logBuffer[LOG_CAPACITY];
size_t logNext = 0, logCount = 0;

String csvRow(const Observation& r) {
  String row;
  row.reserve(140);
  row += String(r.uptimeMs);
  row += "," + String(r.mq6mV, 0) + "," + String(r.mq2mV, 0);
  row += ",";
  if (r.envValid) row += String(r.temperatureC, 2);
  row += ",";
  if (r.envValid) row += String(r.humidityPct, 2);
  row += "," + String(r.envValid ? "1" : "0");
  row += "," + String(r.alarm ? "1" : "0");
  row += "," + String(r.fault ? "1" : "0");
  row += "," + String(r.status);
  row += "," + String(labelText(r.label)) + "\n";
  return row;
}


bool warmingUp(uint32_t now) {
  return (uint32_t)(now - bootAt) < RESTART_WARMUP_MS;
}

// The millivolt API is calibrated by the ESP32 framework where supported.
// Convert ADC-side voltage back to the module's approximate AO voltage.
float readSensorMillivolts(uint8_t pin) {
  uint32_t sum = 0;
  constexpr int N = 12;
  for (int i = 0; i < N; ++i) {
    sum += analogReadMilliVolts(pin);
    delayMicroseconds(150);
  }
  return (sum / float(N)) * AO_TO_ADC_RATIO;
}

bool looksLikeGasRise() {
  const enose::Rules rules = {
    MQ6_STRONG_RATIO, MQ6_STRONG_DELTA_MV,
    MQ2_STRONG_RATIO, MQ2_STRONG_DELTA_MV,
    BOTH_RATIO, BOTH_DELTA_MV
  };
  return enose::looksLikeGasRise(
      mq6mV, mq2mV, baseline6mV, baseline2mV, rules);
}

const char* statusText(uint32_t now) {
  if (sensorFault) return "SENSOR FAULT - ATTENTION";
  if (warmingUp(now)) return "WARMING UP - NOT READY";
  if (calibrating) return "CALIBRATING - NOT READY";
  if (!haveBaseline) return "CALIBRATION REQUIRED - NOT READY";
  if (alarmLatched) return "ALARM LATCHED - CHECK FOR GAS";
  return "MONITORING (PROTOTYPE ONLY)";
}

void startCalibration(uint32_t now) {
  if (warmingUp(now) || sensorFault || alarmLatched) {
    Serial.println("Calibration blocked: warmup, fault or alarm active.");
    return;
  }
  calibrating = true;
  calibrationCount = 0;
  calibrationSum6 = 0.0f;
  calibrationSum2 = 0.0f;
  confirmCount = 0;
  safeSince = 0;
  Serial.println("CALIBRATING. Verify CLEAN AIR. Takes 60 samples.");
  Serial.println("Do NOT calibrate during a suspected leak.");
}

void tryResetAlarm(uint32_t now) {
  if (!alarmLatched) {
    Serial.println("No latched alarm.");
    return;
  }
  if (sensorFault || warmingUp(now) || calibrating || !haveBaseline) {
    Serial.println("Cannot reset: system not ready or sensor fault.");
    return;
  }
  if (safeSince != 0 && (uint32_t)(now - safeSince) >= SAFE_TO_RESET_MS) {
    alarmLatched = false;
    confirmCount = 0;
    safeSince = 0;
    Serial.println("Alarm reset. Continue investigating the cause.");
  } else {
    Serial.println("Reset blocked: readings must stay below thresholds for 30 s.");
  }
}

void recordReading(uint32_t now) {
  Observation rec{now, mq6mV, mq2mV, environment.temperatureC,
                  environment.humidityPct, environment.valid,
                  alarmLatched, sensorFault, statusText(now), experimentLabel};
  logBuffer[logNext] = rec;
  logNext = (logNext + 1) % LOG_CAPACITY;
  if (logCount < LOG_CAPACITY) ++logCount;
  Serial.print("CSV,");
  Serial.print(csvRow(rec));
}

void sampleSensors(uint32_t now) {
  mq6mV = readSensorMillivolts(PIN_MQ6);
  mq2mV = readSensorMillivolts(PIN_MQ2);

  activeFault = healthMonitor.update(mq6mV, mq2mV);
  sensorFault = activeFault != enose::SensorFault::None;

  if (sensorFault) {
    if (haveBaseline) alarmLatched = true; // Keep a known system fault latched
    if (calibrating) {
      calibrating = false;
      Serial.println("Calibration aborted: suspect sensor signal.");
    }
    safeSince = 0;
  } else if (warmingUp(now)) {
    // These samples are observed but are not treated as reliable detection.
  } else if (calibrating) {
    calibrationSum6 += mq6mV;
    calibrationSum2 += mq2mV;
    ++calibrationCount;
    if (calibrationCount >= CALIBRATION_SAMPLES) {
      float new6 = calibrationSum6 / CALIBRATION_SAMPLES;
      float new2 = calibrationSum2 / CALIBRATION_SAMPLES;
      if (new6 > MIN_VALID_SENSOR_MV && new2 > MIN_VALID_SENSOR_MV) {
        baseline6mV = new6;
        baseline2mV = new2;
        bool ok6 = (prefs.putFloat("base6", new6) == sizeof(float));
        bool ok2 = (prefs.putFloat("base2", new2) == sizeof(float));
        haveBaseline = ok6 && ok2;
        Serial.printf("Baseline MQ6=%.0f mV, MQ2=%.0f mV\n", new6, new2);
        Serial.println(haveBaseline ? "Saved calibration." :
                                      "Flash save failed. Recalibrate after repair.");
      }
      calibrating = false;
    }
  } else if (haveBaseline) {
    bool gasLike = looksLikeGasRise();
    confirmCount = gasLike ? min(confirmCount + 1, ALARM_CONFIRM_SAMPLES) : 0;
    if (confirmCount >= ALARM_CONFIRM_SAMPLES) {
      if (!alarmLatched) Serial.println("ALARM: significant gas-sensor rise!");
      alarmLatched = true;
    }
    if (alarmLatched) {
      if (gasLike) {
        safeSince = 0;
      } else if (safeSince == 0) {
        safeSince = now;
      }
    }
  }

  Serial.printf("MQ6=%6.0f mV, MQ2=%6.0f mV | base6=%6.0f, base2=%6.0f | %s | health=%s\n",
                mq6mV, mq2mV, baseline6mV, baseline2mV, statusText(now),
                enose::sensorFaultName(activeFault));
  recordReading(now);
}

// SHT31: single-shot high repeatability command 0x2400, no clock stretching.
// A failed environmental reading does NOT silence the independent MQ alert.
void sampleEnvironment() {
  Wire.beginTransmission(SHT31_ADDRESS);
  Wire.write(0x24);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    environment = enose::Environment{};
    sht31Present = false;
    return;
  }
  delay(20); // Conversion time for high repeatability
  uint8_t data[6] = {0};
  if (Wire.requestFrom(int(SHT31_ADDRESS), 6) != 6) {
    environment = enose::Environment{};
    sht31Present = false;
    return;
  }
  for (int i = 0; i < 6; ++i) data[i] = uint8_t(Wire.read());
  environment = enose::parseSht31(data, 6);
  sht31Present = environment.valid;
}

void updateIndicators(uint32_t now) {
  if (sensorFault) {
    // 250 ms pulse every 1.5 seconds, red flashes slowly.
    bool pulse = (now % 1500UL) < 250UL;
    digitalWrite(PIN_BUZZER, pulse ? HIGH : LOW);
    digitalWrite(PIN_RED, pulse ? HIGH : LOW);
    digitalWrite(PIN_GREEN, LOW);
  } else if (alarmLatched) {
    // Gas alert: repeated beeps; works even when Wi-Fi is disconnected.
    digitalWrite(PIN_BUZZER, (now % 500UL) < 250UL ? HIGH : LOW);
    digitalWrite(PIN_RED, (now % 500UL) < 250UL ? HIGH : LOW);
    digitalWrite(PIN_GREEN, LOW);
  } else if (warmingUp(now) || calibrating || !haveBaseline) {
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_RED, HIGH);
    digitalWrite(PIN_GREEN, LOW);
  } else {
    digitalWrite(PIN_BUZZER, LOW);
    digitalWrite(PIN_RED, LOW);
    digitalWrite(PIN_GREEN, HIGH);
  }
}

void updateButton(uint32_t now) {
  bool raw = digitalRead(PIN_RESET);
  if (raw != previousButtonRaw) {
    previousButtonRaw = raw;
    buttonRawChangedAt = now;
  }
  if ((uint32_t)(now - buttonRawChangedAt) >= BUTTON_DEBOUNCE_MS &&
      raw != debouncedButton) {
    debouncedButton = raw;
    if (raw == LOW) tryResetAlarm(now);
  }
}

bool requireDashboardAuth() {
  // HTTP Basic auth is a convenience gate only; it is not HTTPS.
  // Refuse to serve sensor data when credentials were not explicitly set.
  if (strlen(DASHBOARD_USER) == 0 || strlen(DASHBOARD_PASSWORD) < 12) {
    server.send(503, "text/plain", "Dashboard disabled: configure credentials in secrets.h");
    return false;
  }
  if (!server.authenticate(DASHBOARD_USER, DASHBOARD_PASSWORD)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void handleCsv() {
  if (!requireDashboardAuth()) return;
  server.sendHeader("Content-Disposition", "attachment; filename=\"enose_recent.csv\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv; charset=utf-8", "");
  server.sendContent("uptime_ms,mq6_mv,mq2_mv,temperature_c,humidity_pct,env_valid,alarm,fault,status,activity_label\n");
  const size_t first = (logNext + LOG_CAPACITY - logCount) % LOG_CAPACITY;
  for (size_t i = 0; i < logCount; ++i) {
    server.sendContent(csvRow(logBuffer[(first + i) % LOG_CAPACITY]));
  }
  server.sendContent("");
}

void handleDashboard() {
  if (!requireDashboardAuth()) return;
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", String(DASHBOARD_PAGE));
}

void handleAPI() {
  if (!requireDashboardAuth()) return;
  server.sendHeader("Cache-Control", "no-store");
  uint32_t now = millis();
  String json = "{\"status\":\"" + String(statusText(now)) + "\"";
  json += ",\"uptime_ms\":" + String(now);
  json += ",\"mq6_mV\":" + String(mq6mV, 0);
  json += ",\"mq2_mV\":" + String(mq2mV, 0);
  json += ",\"env_valid\":" + String(environment.valid ? "true" : "false");
  json += ",\"temperature_c\":" + (environment.valid ? String(environment.temperatureC, 2) : String("null"));
  json += ",\"humidity_pct\":" + (environment.valid ? String(environment.humidityPct, 2) : String("null"));
  json += ",\"recorded_rows\":" + String(logCount);
  json += ",\"activity_label\":\"" + String(labelText(experimentLabel)) + "\"";
  json += ",\"alarm\":" + String(alarmLatched ? "true" : "false");
  json += ",\"fault\":" + String(sensorFault ? "true" : "false");
  json += ",\"fault_reason\":\"" + String(enose::sensorFaultName(activeFault)) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleSerial(uint32_t now) {
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '0') { experimentLabel = 'u'; Serial.println("Activity: unlabeled"); }
    if (ch == '1') { experimentLabel = 'b'; Serial.println("Activity: background"); }
    if (ch == '2') { experimentLabel = 'c'; Serial.println("Activity: cooking"); }
    if (ch == '3') { experimentLabel = 'x'; Serial.println("Activity: cleaning"); }
    if (ch == 'c' || ch == 'C') startCalibration(now);
    if (ch == 'r' || ch == 'R') tryResetAlarm(now);
    if (ch == 't' || ch == 'T') {
      if (!warmingUp(now) && !calibrating && haveBaseline) {
        alarmLatched = true;
        safeSince = 0;
        Serial.println("SIMULATED ALARM. No LPG was used.");
      } else {
        Serial.println("Test unavailable until calibration and warmup complete.");
      }
    }
  }
}

void updateWiFi() {
  if (strlen(WIFI_SSID) == 0) return;
  if (WiFi.status() == WL_CONNECTED) {
    if (!webStarted) {
      server.begin();
      webStarted = true;
      Serial.print("Dashboard: http://");
      Serial.println(WiFi.localIP());
    }
    server.handleClient();
  } else {
    webStarted = false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_RESET, INPUT_PULLUP);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_RED, HIGH);
  digitalWrite(PIN_GREEN, LOW);

  Wire.begin(21, 22); // SHT31: SDA=21, SCL=22; 3.3 V I2C module
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MQ6, ADC_11db);
  analogSetPinAttenuation(PIN_MQ2, ADC_11db);

  prefs.begin("lpg-enose", false);
  baseline6mV = prefs.getFloat("base6", NAN);
  baseline2mV = prefs.getFloat("base2", NAN);
  haveBaseline = isfinite(baseline6mV) && isfinite(baseline2mV) &&
                 baseline6mV > MIN_VALID_SENSOR_MV &&
                 baseline2mV > MIN_VALID_SENSOR_MV;

  server.on("/", HTTP_GET, handleDashboard);
  server.on("/api", HTTP_GET, handleAPI);
  server.on("/csv", HTTP_GET, handleCsv);
  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  bootAt = millis();
  lastSampleAt = bootAt - SAMPLE_INTERVAL_MS;
  lastEnvAt = bootAt - ENV_INTERVAL_MS;
  Serial.println("ESP32 LPG E-Nose demo started.");
  Serial.println("Sensor warm-up after restart: 3 minutes.");
  Serial.println("NEW MQ sensors need full initial burn-in per manufacturer.");
  Serial.println("Serial commands: c=calibrate in verified clean air, t=test, r=reset");
  Serial.println("Activity labels: 0=unlabeled, 1=background, 2=cooking, 3=cleaning.");
  Serial.println("CSV,uptime_ms,mq6_mv,mq2_mv,temperature_c,humidity_pct,env_valid,alarm,fault,status,activity_label");
  Serial.println(haveBaseline ? "Saved baseline loaded." :
                                "NO baseline. Calibrate after warm-up in clean air.");
}

void loop() {
  uint32_t now = millis();
  handleSerial(now);
  updateButton(now);
  if ((uint32_t)(now - lastSampleAt) >= SAMPLE_INTERVAL_MS) {
    lastSampleAt = now;
    sampleSensors(now);
  }
  if ((uint32_t)(now - lastEnvAt) >= ENV_INTERVAL_MS) {
    lastEnvAt = now;
    sampleEnvironment();
  }
  updateIndicators(now);
  updateWiFi();
  delay(2); // Yield to Wi-Fi tasks without blocking alarm timing significantly.
}