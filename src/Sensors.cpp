#include "Sensors.h"
#include "Config.h"

int readAveragedAdc(uint8_t pin) {
  int samples[10];
  for (int i = 0; i < 10; ++i) {
    samples[i] = analogRead(pin);
    delay(2);
  }
  // Trimmed mean: bo 2 mau thap nhat va 2 mau cao nhat de cat xung nhieu.
  for (int i = 1; i < 10; ++i) {
    const int key = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > key) { samples[j + 1] = samples[j]; --j; }
    samples[j + 1] = key;
  }
  long sum = 0;
  for (int i = 2; i < 8; ++i) sum += samples[i];
  return static_cast<int>(sum / 6);
}

namespace {
float emaSoil = -1.0f;
float emaRain = -1.0f;
constexpr float EMA_ALPHA = 0.3f;

float ema(float &state, float sample) {
  state = (state < 0.0f) ? sample : state + EMA_ALPHA * (sample - state);
  return state;
}
}  // namespace

int soilPercentFromRaw(int raw) {
  if (SOIL_RAW_DRY == SOIL_RAW_WET) return 0;
  const long percent = static_cast<long>(raw - SOIL_RAW_DRY) * 100L /
                       (SOIL_RAW_WET - SOIL_RAW_DRY);
  return constrain(static_cast<int>(percent), 0, 100);
}

void readSensors(AppState& state) {
  state.soilRaw = readAveragedAdc(SOIL_PIN);
  state.soilBase = soilPercentFromRaw(static_cast<int>(ema(emaSoil, state.soilRaw)));

  const int rainRaw = readAveragedAdc(RAIN_AO_PIN);
  state.rainPot = constrain(map(static_cast<int>(ema(emaRain, rainRaw)), 0, 4095, 0, 100), 0, 100);
  state.rainingNow = digitalRead(RAIN_DO_PIN) == LOW;
}

