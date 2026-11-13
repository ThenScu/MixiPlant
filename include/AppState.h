#pragma once

#include <Arduino.h>

#include "Config.h"

struct AppState {
  int soilBase = 0;
  int soilRaw = 0;
  float irrigBoost = 0;
  int currentMoisture = 0;

  int rainApi = -1;  // -1 = chua co forecast, khong gia lap xac suat mua
  int rainPot = 0;
  int rainEff = 20;
  bool rainingNow = false;
  float airTemp = 0;
  int airHum = 0;
  bool weatherValid = false;
  bool mqttOnline = false;
  unsigned long lastWeatherSuccess = 0;
  unsigned long lastWeatherCheck = 0;

  bool pumpActive = false;
  String statusText = "KHOI DONG";
  String pumpMode = "AUTO";
  String lastCommandId = "";
  String lastCommandResult = "NONE";
  int rainTestVal = -1;
  unsigned long rainTestUntil = 0;
  int rainTestBasePot = 0;
  int soilTestVal = -1;
  unsigned long soilTestUntil = 0;
  int soilTestBasePct = 0;

  // Ring buffer telemetry khi mat MQTT; dong bo lai khi co mang (2B).
  String ring[TELEMETRY_RING_CAPACITY];
  size_t ringHead = 0;
  size_t ringCount = 0;

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

