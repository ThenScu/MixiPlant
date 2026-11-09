#include "NetworkService.h"

#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "Config.h"

namespace {
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
AppState* appState = nullptr;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  (void)topic;
  if (appState == nullptr) return;

  String message;
  for (unsigned int i = 0; i < length; ++i) message += static_cast<char>(payload[i]);
  message.trim();
  message.toUpperCase();
  Serial.printf("[MQTT] Lenh nhan: %s\n", message.c_str());

  if (message == "ON") {
    appState->pumpMode = "MANUAL_ON";
  } else if (message == "OFF") {
    appState->pumpMode = "MANUAL_OFF";
  } else if (message.startsWith("RAIN:")) {
    const int value = constrain(message.substring(5).toInt(), 0, 100);
    appState->rainTestVal = value;
    appState->rainTestUntil = millis() + RAIN_TEST_TIMEOUT_MS;
    Serial.printf("[Test] Ep mua=%d%% trong 2 phut\n", value);
  } else if (message == "AUTO") {
    appState->pumpMode = "AUTO";
  }
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
    if (mqtt.connect(clientId.c_str())) {
      mqtt.subscribe(TOPIC_SUB);
      state.mqttRetryDelay = MQTT_RETRY_MIN_MS;
      Serial.println("[MQTT] Da ket noi va subscribe lenh");
    } else {
      state.mqttRetryDelay = min(state.mqttRetryDelay * 2, MQTT_RETRY_MAX_MS);
      Serial.printf("[MQTT] Loi %d, thu lai sau %lu ms\n",
                    mqtt.state(), state.mqttRetryDelay);
    }
  }

  mqtt.loop();
}

void publishTelemetry(const AppState& state, unsigned long now) {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["am_goc"] = state.soilBase;
  doc["am_raw"] = state.soilRaw;
  doc["npk_raw"] = state.npkRaw;
  doc["npk_demo_mgkg"] = state.npkDemoMgKg;
  doc["ec_raw"] = state.ecRaw;
  doc["ec_demo_us_cm"] = state.ecDemoUsCm;
  doc["am"] = state.currentMoisture;
  doc["mua_api"] = state.rainApi;
  doc["mua_pot"] = state.rainPot;
  doc["mua"] = state.rainEff;
  doc["du_bao_hop_le"] = state.weatherValid &&
                          now - state.lastWeatherSuccess < WEATHER_MAX_AGE_MS;
  doc["troi_mua"] = state.rainingNow;
  doc["bom"] = state.pumpActive ? "ON" : "OFF";
  doc["chedo"] = state.statusText;
  doc["wifi"] = WiFi.status() == WL_CONNECTED;
  doc["mqtt"] = mqtt.connected();

  String output;
  serializeJson(doc, output);
  mqtt.publish(TOPIC_PUB, output.c_str());
}

