#pragma once

#include <Arduino.h>

struct AppState {
  int soilBase = 0;
  int soilRaw = 0;
  int npkRaw = 0;
  int ecRaw = 0;
  int npkDemoMgKg = 0;
  int ecDemoUsCm = 0;
  float irrigBoost = 0;
  int currentMoisture = 0;

  int rainApi = 20;
  int rainPot = 0;
  int rainEff = 20;
  bool rainingNow = false;
  float airTemp = 0;
  int airHum = 0;
  bool weatherValid = false;
  unsigned long lastWeatherSuccess = 0;
  unsigned long lastWeatherCheck = 0;

  bool pumpActive = false;
  String statusText = "KHOI DONG";
  String pumpMode = "AUTO";
  int rainTestVal = -1;
  unsigned long rainTestUntil = 0;

  unsigned long lastPumpChange = 0;
  unsigned long lastMqttPub = 0;
  unsigned long lastLcd = 0;
  unsigned long lastSecond = 0;
  unsigned long lastWifiAttempt = 0;
  unsigned long lastMqttAttempt = 0;
  unsigned long mqttRetryDelay = 5000UL;

  bool irrigationCycle = false;
  bool pulseIsOn = false;
  unsigned long pulsePhaseStarted = 0;
};

