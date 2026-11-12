#include <Arduino.h>
#include <WiFi.h>

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
    app.rainEff = app.rainTestVal;
    return;
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

void updateWokwiSoilModel(unsigned long now) {
  if (now - app.lastSecond < 1000UL) return;
  app.lastSecond = now;

  float delta = 0;
  if (app.pumpActive) delta += 2.0f;
  else delta -= 1.0f;

  if (app.rainingNow) delta += 1.5f;
  else if (app.rainEff >= RAIN_THRESHOLD) delta += 1.0f;
  else if (app.rainEff >= RAIN_LIGHT) delta += 0.3f;

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
  selectEffectiveRain(now);
  if (SIMULATE_SOIL_RESPONSE) {
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

  if (now - app.lastMqttPub >= 5000UL) {
    app.lastMqttPub = now;
    publishTelemetry(app, now);
  }

  enterLowPowerSleepIfSafe(app);
}
