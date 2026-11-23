#include "PowerManager.h"

#include <Arduino.h>
#include <esp_attr.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "Config.h"

namespace {
RTC_DATA_ATTR bool resumeAutoAfterDeepSleep = false;
}

void rememberPumpModeBeforeDeepSleep(const AppState& state) {
  resumeAutoAfterDeepSleep = state.pumpMode == "AUTO";
}

void restorePumpModeAfterDeepSleep(AppState& state) {
  state.pumpMode = resumeAutoAfterDeepSleep ? "AUTO" : "MANUAL_OFF";
  resumeAutoAfterDeepSleep = false;
}

void enterDeepSleepDemoIfRequested(AppState& state, unsigned long now) {
  if (!state.deepSleepDemoRequested) return;
  if (state.pumpActive) {
    state.deepSleepDemoRequested = false;
    Serial.println("[Demo] Ngu bi huy vi bom dang chay.");
    return;
  }
  if (now - state.deepSleepDemoRequestedAt < DEEP_SLEEP_DEMO_PUBLISH_WAIT_MS) return;

  Serial.println("[Power] Vao Deep Sleep. Thoi gian danh thuc da cau hinh trong firmware.");
  Serial.flush();

  // Đưa đầu ra về trạng thái an toàn trước khi CPU/peripheral tắt.
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(VALVE_RELAY_PIN, LOW);
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);

  esp_sleep_enable_timer_wakeup(
      static_cast<uint64_t>(DEEP_SLEEP_DEMO_SECONDS) * 1000000ULL);
  esp_deep_sleep_start();
}
