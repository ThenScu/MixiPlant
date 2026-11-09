#include "Sensors.h"
#include "Config.h"

int readAveragedAdc(uint8_t pin) {
  long sum = 0;
  for (int i = 0; i < 10; ++i) {
    sum += analogRead(pin);
    delay(2);
  }
  return static_cast<int>(sum / 10);
}

int soilPercentFromRaw(int raw) {
  if (SOIL_RAW_DRY == SOIL_RAW_WET) return 0;
  const long percent = static_cast<long>(raw - SOIL_RAW_DRY) * 100L /
                       (SOIL_RAW_WET - SOIL_RAW_DRY);
  return constrain(static_cast<int>(percent), 0, 100);
}

void readSensors(AppState& state) {
  state.soilRaw = readAveragedAdc(SOIL_PIN);
  state.soilBase = soilPercentFromRaw(state.soilRaw);

  state.rainPot = constrain(map(readAveragedAdc(RAIN_AO_PIN), 0, 4095, 0, 100), 0, 100);
  state.npkRaw = readAveragedAdc(NPK_PIN);
  state.ecRaw = readAveragedAdc(EC_PIN);

  // Gia tri demo tu potentiometer, khong phai so do NPK/EC tu cam bien that.
  state.npkDemoMgKg = map(state.npkRaw, 0, 4095, 0, 2000);
  state.ecDemoUsCm = map(state.ecRaw, 0, 4095, 0, 5000);
  state.rainingNow = digitalRead(RAIN_DO_PIN) == LOW;
}

