#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "AppState.h"
#include "Config.h"
#include "Display.h"
#include "IrrigationController.h"
#include "NetworkService.h"
#include "PowerManager.h"
#include "Sensors.h"
#include "WeatherService.h"

namespace {
AppState app;

void selectEffectiveRain(unsigned long now) {
  if (app.rainTestVal >= 0 && now < app.rainTestUntil) {
    if (abs(app.rainPot - app.rainTestBasePot) > POT_OVERRIDE_CANCEL_DELTA) {
      app.rainTestVal = -1;  // vuot num MUA tren mach: mach thang override
    } else {
      app.rainEff = app.rainTestVal;
      return;
    }
  }

  app.rainTestVal = -1;
  if (app.rainingNow) {
    app.rainEff = 95;
  } else if (abs(app.rainPot - app.rainApi) > 15) {
    app.rainEff = app.rainPot;  // Potentiometer MUA TEST tren Wokwi
  } else if (app.weatherValid) {
    app.rainEff = app.rainApi;
  } else {
    app.rainEff = 0;  // offline: khong de forecast cu chan quyet dinh cuc bo
  }
}

bool applyDemoOverrides(unsigned long now) {
  if (app.soilTestVal >= 0 && now < app.soilTestUntil) {
    if (abs(app.soilBase - app.soilTestBasePct) > POT_OVERRIDE_CANCEL_DELTA) {
      app.soilTestVal = -1;  // vuot num DO AM DAT tren mach: mach thang override
    } else {
      return true;
    }
  } else {
    app.soilTestVal = -1;
  }
  return false;
}

void updateWokwiSoilModel(unsigned long now) {
  if (now - app.lastSecond < 1000UL) return;
  app.lastSecond = now;

  float delta = 0;
  if (app.pumpActive) delta += 1.0f;
  else delta -= 0.05f;

  if (app.rainingNow) delta += 0.5f;
  else if (app.rainEff >= RAIN_THRESHOLD) delta += 0.5f;
  else if (app.rainEff >= RAIN_LIGHT) delta += 0.1f;

  app.irrigBoost += delta;
  app.irrigBoost = constrain(app.irrigBoost, -40.0f, 80.0f);
}

void applyActuators() {
  digitalWrite(RELAY_PIN, app.pumpActive ? HIGH : LOW);
  digitalWrite(VALVE_RELAY_PIN, app.pumpActive ? HIGH : LOW);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    restorePumpModeAfterDeepSleep(app);
    Serial.printf("[Power] Het gio Deep Sleep. Khoi dong lai, che do duoc khoi phuc: %s.\n",
                  app.pumpMode.c_str());
  } else {
    Serial.println("[MixiPlant] Khoi dong ESP32.");
  }

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(VALVE_RELAY_PIN, OUTPUT);
  pinMode(RAIN_DO_PIN, INPUT_PULLUP);
  applyActuators();  // safe state on startup: pump and valve off

  beginDisplay();
  beginNetworkService(app);
  beginWeatherService(app);
}

void loop() {
  const unsigned long now = millis();

  updateNetworkService(app, now);
  updateWeather(app, now);

  readSensors(app);
  const bool soilOverrideActive = applyDemoOverrides(now);
  selectEffectiveRain(now);
  if (soilOverrideActive) {
    // Dùng slider làm mốc ban đầu, sau đó mô phỏng phản hồi của bơm/mưa.
    // Không cần bấm "Trả về mạch" để độ ẩm tiếp tục thay đổi.
    updateWokwiSoilModel(now);
    app.currentMoisture = constrain(
        static_cast<int>(app.soilTestVal + app.irrigBoost), 0, 100);
  } else if (SIMULATE_SOIL_RESPONSE) {
    updateWokwiSoilModel(now);
    app.currentMoisture = constrain(
        static_cast<int>(app.soilBase + app.irrigBoost), 0, 100);
  } else {
    app.irrigBoost = 0;
    app.currentMoisture = app.soilBase;
  }

  updateIrrigationDecision(app, now);
  applyActuators();
  updateDisplay(app, now, WiFi.status() == WL_CONNECTED);

  if (now - app.lastMqttPub >= TELEMETRY_PUBLISH_INTERVAL_MS) {
    app.lastMqttPub = now;
    publishTelemetry(app, now);
  }
  enterDeepSleepDemoIfRequested(app, now);
}
