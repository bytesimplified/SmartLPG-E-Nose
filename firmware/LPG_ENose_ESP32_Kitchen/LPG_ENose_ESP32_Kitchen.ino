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

// ---- Wi-Fi: replace these, or leave blank to use Serial only ----
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// ---- Pin mapping (classic ESP32, NOT ESP32-C3/S3) ----
constexpr uint8_t PIN_MQ6    = 34;
constexpr uint8_t PIN_MQ2    = 35;
constexpr uint8_t PIN_BUZZER = 25;
constexpr uint8_t PIN_RED    = 26;
constexpr uint8_t PIN_GREEN  = 27;
constexpr uint8_t PIN_RESET  = 33;

// A 10k series resistor + 10k to GND means the ADC sees half AO.
constexpr float AO_TO_ADC_RATIO = 2.0f;

// ---- Timing ----
constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t RESTART_WARMUP_MS = 180000;   // 3 min, NOT initial sensor burn-in
constexpr uint32_t SAFE_TO_RESET_MS = 30000;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;
constexpr int CALIBRATION_SAMPLES = 60;          // 60 x 1 sec = ~60 sec
constexpr int ALARM_CONFIRM_SAMPLES = 3;

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
uint32_t safeSince = 0;
uint32_t buttonRawChangedAt = 0;

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
int invalidCount = 0;

bool previousButtonRaw = HIGH;
bool debouncedButton = HIGH;

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
  if (!haveBaseline) return false;
  float delta6 = mq6mV - baseline6mV;
  float delta2 = mq2mV - baseline2mV;

  bool mq6Strong = mq6mV >= baseline6mV * MQ6_STRONG_RATIO &&
                   delta6 >= MQ6_STRONG_DELTA_MV;
  bool mq2Strong = mq2mV >= baseline2mV * MQ2_STRONG_RATIO &&
                   delta2 >= MQ2_STRONG_DELTA_MV;
  bool bothRising = mq6mV >= baseline6mV * BOTH_RATIO &&
                    mq2mV >= baseline2mV * BOTH_RATIO &&
                    delta6 >= BOTH_DELTA_MV &&
                    delta2 >= BOTH_DELTA_MV;

  // Do not require both sensors to detect gas: MQ-6 can alarm on its own.
  return mq6Strong || mq2Strong || bothRising;
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

void sampleSensors(uint32_t now) {
  mq6mV = readSensorMillivolts(PIN_MQ6);
  mq2mV = readSensorMillivolts(PIN_MQ2);

  bool invalid = !isfinite(mq6mV) || !isfinite(mq2mV) ||
                 mq6mV < MIN_VALID_SENSOR_MV ||
                 mq2mV < MIN_VALID_SENSOR_MV;
  invalidCount = invalid ? min(invalidCount + 1, 5) : 0;
  sensorFault = (invalidCount >= 5);

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

  Serial.printf("MQ6=%6.0f mV, MQ2=%6.0f mV | base6=%6.0f, base2=%6.0f | %s\n",
                mq6mV, mq2mV, baseline6mV, baseline2mV, statusText(now));
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

void handleDashboard() {
  uint32_t now = millis();
  String html;
  html.reserve(1600);
  html += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<meta http-equiv='refresh' content='3'><title>LPG E-Nose</title>");
  html += F("<style>body{font-family:system-ui;margin:22px;max-width:550px;background:#f6f7f9}");
  html += F("article{background:white;padding:20px;border-radius:12px;margin-top:14px}");
  html += F("h1{font-size:24px}strong{font-size:20px}</style></head><body>");
  html += F("<h1>Kitchen E-Nose prototype</h1><article><p><strong>");
  html += statusText(now);
  html += F("</strong></p><p>MQ-6 signal: ");
  html += String(mq6mV, 0);
  html += F(" mV</p><p>MQ-2 signal: ");
  html += String(mq2mV, 0);
  html += F(" mV</p><p>MQ-6 baseline: ");
  html += haveBaseline ? String(baseline6mV, 0) : String("N/A");
  html += F(" mV</p><p>MQ-2 baseline: ");
  html += haveBaseline ? String(baseline2mV, 0) : String("N/A");
  html += F(" mV</p></article><p>Refreshes every 3 seconds.</p>");
  html += F("<p>This is NOT a certified gas alarm. Use a separate certified LPG detector.</p>");
  html += F("</body></html>");
  server.send(200, "text/html; charset=utf-8", html);
}

void handleAPI() {
  uint32_t now = millis();
  String json = "{\"status\":\"" + String(statusText(now)) + "\"";
  json += ",\"mq6_mV\":" + String(mq6mV, 0);
  json += ",\"mq2_mV\":" + String(mq2mV, 0);
  json += ",\"alarm\":" + String(alarmLatched ? "true" : "false");
  json += ",\"fault\":" + String(sensorFault ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleSerial(uint32_t now) {
  while (Serial.available()) {
    char ch = Serial.read();
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
  if (strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  bootAt = millis();
  lastSampleAt = bootAt - SAMPLE_INTERVAL_MS;
  Serial.println("ESP32 LPG E-Nose demo started.");
  Serial.println("Sensor warm-up after restart: 3 minutes.");
  Serial.println("NEW MQ sensors need full initial burn-in per manufacturer.");
  Serial.println("Serial commands: c=calibrate in verified clean air, t=test, r=reset");
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
  updateIndicators(now);
  updateWiFi();
  delay(2); // Yield to Wi-Fi tasks without blocking alarm timing significantly.
}