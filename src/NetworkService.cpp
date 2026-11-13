#include "NetworkService.h"

#include <WiFi.h>
#if MIXIPLAN_MQTT_TLS
#include <WiFiClientSecure.h>
#endif
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <string.h>

#include "Config.h"

namespace {
#if MIXIPLAN_MQTT_TLS
WiFiClientSecure wifiClient;
#else
WiFiClient wifiClient;
#endif
PubSubClient mqtt(wifiClient);
AppState* appState = nullptr;

// Goi tin gon cho ring buffer / LoRa: key ngan, bo truong lap (2B.1).
String compactTelemetry(const AppState& s, unsigned long now) {
  JsonDocument d;
  d["bf"] = 1;
  d["t"] = now;
  d["ag"] = s.soilBase;
  d["ar"] = s.soilRaw;
  d["a"] = s.currentMoisture;
  d["ma"] = s.rainApi;
  d["mp"] = s.rainPot;
  d["m"] = s.rainEff;
  d["tm"] = s.rainingNow;
  d["b"] = s.pumpActive ? 1 : 0;
  d["c"] = s.statusText;
  d["w"] = WiFi.status() == WL_CONNECTED ? 1 : 0;
  String out;
  serializeJson(d, out);
  return out;
}

void ringPush(AppState& s, const String& frame) {
  s.ring[s.ringHead] = frame;
  s.ringHead = (s.ringHead + 1) % TELEMETRY_RING_CAPACITY;
  if (s.ringCount < TELEMETRY_RING_CAPACITY) s.ringCount++;
}

void flushRing(AppState& s) {
  while (s.ringCount > 0 && mqtt.connected()) {
    const size_t idx =
        (s.ringHead + TELEMETRY_RING_CAPACITY - s.ringCount) % TELEMETRY_RING_CAPACITY;
    mqtt.publish(TOPIC_PUB, s.ring[idx].c_str());
    s.ring[idx] = String();
    s.ringCount--;
  }
  if (s.ringCount == 0) s.ringHead = 0;
}

void publishCommandAck(const String& commandId, const String& requestedState,
                       bool accepted, const char* reason) {
  if (!mqtt.connected()) return;
  JsonDocument ack;
  ack["id"] = commandId;
  ack["device"] = "pump_and_valve";
  ack["requested"] = requestedState;
  ack["accepted"] = accepted;
  ack["reason"] = reason;
  String payload;
  serializeJson(ack, payload);
  mqtt.publish(TOPIC_ACK, payload.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  (void)topic;
  if (appState == nullptr) return;

  String message;
  for (unsigned int i = 0; i < length; ++i) message += static_cast<char>(payload[i]);
  message.trim();
  String commandId;
  String requestedState;
  String target = "pump";

  if (message.startsWith("{")) {
    JsonDocument command;
    if (deserializeJson(command, message)) {
      publishCommandAck("", "", false, "INVALID_JSON");
      return;
    }
    commandId = command["id"] | "";
    target = command["device"] | "pump";
    requestedState = command["state"] | "";
  } else {
    requestedState = message;
  }
  requestedState.trim();
  requestedState.toUpperCase();
  target.toLowerCase();

  const bool knownTarget = target == "pump" || target == "pump_and_valve";
  if (!knownTarget) {
    if (appState) {
      appState->lastCommandId = commandId;
      appState->lastCommandResult = "REJECTED_TARGET";
    }
    publishCommandAck(commandId, requestedState, false, "UNKNOWN_DEVICE");
    return;
  }

  bool accepted = true;
  if (requestedState == "ON") {
    appState->pumpMode = "MANUAL_ON";
  } else if (requestedState == "OFF") {
    appState->pumpMode = "MANUAL_OFF";
  } else if (requestedState == "AUTO") {
    appState->pumpMode = "AUTO";
  } else if (requestedState.startsWith("RAIN:")) {
    const int value = requestedState.substring(5).toInt();
    if (value < 0) {
      appState->rainTestVal = -1;
    } else {
      appState->rainTestVal = constrain(value, 0, 100);
      appState->rainTestUntil = millis() + RAIN_TEST_TIMEOUT_MS;
      appState->rainTestBasePot = appState->rainPot;
    }
    Serial.printf("[Test] Ep mua=%d%% trong 2 phut\n", value);
  } else if (requestedState.startsWith("SOIL:")) {
    const int value = requestedState.substring(5).toInt();
    if (value < 0) {
      appState->soilTestVal = -1;
    } else {
      appState->soilTestVal = constrain(value, 0, 100);
      appState->soilTestUntil = millis() + SOIL_TEST_TIMEOUT_MS;
      appState->soilTestBasePct = appState->soilBase;
    }
    Serial.printf("[Test] Ep do am=%d%% trong 2 phut\n", value);
  } else {
    accepted = false;
  }

  appState->lastCommandId = commandId;
  appState->lastCommandResult = accepted ? "ACCEPTED" : "REJECTED_STATE";
  Serial.printf("[MQTT] Lenh %s: %s\n", accepted ? "chap nhan" : "tu choi",
                requestedState.c_str());
  publishCommandAck(commandId, requestedState, accepted,
                    accepted ? "QUEUED" : "UNKNOWN_STATE");
  // Lenh sai khong thay doi che do dang chay.
}
}  // namespace

void beginNetworkService(AppState& state) {
  appState = &state;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  state.lastWifiAttempt = millis();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(2);

#if MIXIPLAN_MQTT_TLS
  if (strlen(MQTT_CA_CERT) == 0) {
    Serial.println("[MQTT] TLS duoc bat nhung chua co CA cert; MQTT se khong the ket noi");
  } else {
    wifiClient.setCACert(MQTT_CA_CERT);
    if (strlen(MQTT_CLIENT_CERT) > 0 && strlen(MQTT_PRIVATE_KEY) > 0) {
      wifiClient.setCertificate(MQTT_CLIENT_CERT);
      wifiClient.setPrivateKey(MQTT_PRIVATE_KEY);
    }
  }
#endif
}

void updateNetworkService(AppState& state, unsigned long now) {
  if (WiFi.status() != WL_CONNECTED && now - state.lastWifiAttempt >= WIFI_RETRY_MS) {
    state.lastWifiAttempt = now;
    WiFi.reconnect();
    Serial.println("[WiFi] Mat ket noi, dang thu lai");
  }

  if (!mqtt.connected() && WiFi.status() == WL_CONNECTED &&
      now - state.lastMqttAttempt >= state.mqttRetryDelay) {
    state.lastMqttAttempt = now;
    const String clientId = "ESP32-MixiPlan-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    const bool connected = strlen(MQTT_USERNAME) > 0
        ? mqtt.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)
        : mqtt.connect(clientId.c_str());
    if (connected) {
      mqtt.subscribe(TOPIC_SUB);
      state.mqttRetryDelay = MQTT_RETRY_MIN_MS;
      Serial.println("[MQTT] Da ket noi va subscribe lenh");
      if (state.ringCount > 0) {
        Serial.printf("[Sync] Dong bo lai %u goi tin da dem\n",
                      static_cast<unsigned>(state.ringCount));
        flushRing(state);
      }
    } else {
      state.mqttRetryDelay = min(state.mqttRetryDelay * 2, MQTT_RETRY_MAX_MS);
      Serial.printf("[MQTT] Loi %d, thu lai sau %lu ms\n",
                    mqtt.state(), state.mqttRetryDelay);
    }
  }

  mqtt.loop();
  state.mqttOnline = mqtt.connected();
}

void publishTelemetry(AppState& state, unsigned long now) {
  if (!mqtt.connected()) {
    ringPush(state, compactTelemetry(state, now));
    return;
  }

  JsonDocument doc;
  doc["am_goc"] = state.soilBase;
  doc["am_raw"] = state.soilRaw;
  doc["am"] = state.currentMoisture;
  doc["mua_api"] = state.rainApi;
  doc["du_lieu_mo_phong"] = IS_WOKWI_SIMULATION;
  doc["mua_pot"] = state.rainPot;
  doc["mua"] = state.rainEff;
  doc["du_bao_hop_le"] = state.weatherValid &&
                          now - state.lastWeatherSuccess < WEATHER_MAX_AGE_MS;
  doc["troi_mua"] = state.rainingNow;
  doc["nhiet_do"] = state.airTemp;
  doc["do_am_kk"] = state.airHum;
  doc["bom"] = state.pumpActive ? "ON" : "OFF";
  doc["chedo"] = state.statusText;
  doc["lenh_id"] = state.lastCommandId;
  doc["lenh_ketqua"] = state.lastCommandResult;
  doc["wifi"] = WiFi.status() == WL_CONNECTED;
  doc["mqtt"] = mqtt.connected();
  doc["soil_override"] = state.soilTestVal;
  doc["rain_override"] = state.rainTestVal;

  String output;
  serializeJson(doc, output);
  mqtt.publish(TOPIC_PUB, output.c_str());
}

