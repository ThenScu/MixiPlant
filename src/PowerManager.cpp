#include "PowerManager.h"

#include <Arduino.h>
#include <esp_sleep.h>

#include "Config.h"

void enterLowPowerSleepIfSafe(const AppState& state) {
  if (!ENABLE_LOW_POWER_DEEP_SLEEP || SIMULATE_SOIL_RESPONSE) return;
  if (state.pumpMode != "AUTO" || state.pumpActive || state.rainingNow ||
      state.currentMoisture < TARGET_MOISTURE) return;

  Serial.printf("[Power] Dat du am (%d%%); ngu %lu giay roi do lai\n",
                state.currentMoisture, LOW_POWER_WAKE_INTERVAL_MS / 1000UL);
  Serial.flush();
  esp_sleep_enable_timer_wakeup(
      static_cast<uint64_t>(LOW_POWER_WAKE_INTERVAL_MS) * 1000ULL);
  esp_deep_sleep_start();
}
