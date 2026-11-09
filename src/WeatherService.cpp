#include "WeatherService.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <string.h>

#include "Config.h"

namespace {
void fetchWeather(AppState& state) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(5000);
  http.begin("https://api.open-meteo.com/v1/forecast?latitude=10.82&longitude=106.63&current=temperature_2m,relative_humidity_2m&hourly=precipitation_probability&forecast_days=1&timezone=Asia%2FBangkok");
  const int code = http.GET();

  if (code == HTTP_CODE_OK) {
    const String payload = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      const char* currentTime = doc["current"]["time"] | "";
      JsonArray times = doc["hourly"]["time"].as<JsonArray>();
      JsonArray probabilities = doc["hourly"]["precipitation_probability"].as<JsonArray>();
      bool gotProbability = false;

      if (strlen(currentTime) >= 13) {
        const String currentHour = String(currentTime).substring(0, 13);
        for (size_t i = 0; i < times.size() && i < probabilities.size(); ++i) {
          const char* forecastTime = times[i] | "";
          if (strncmp(forecastTime, currentHour.c_str(), 13) == 0) {
            const int probability = probabilities[i] | -1;
            if (probability >= 0 && probability <= 100) {
              state.rainApi = probability;
              gotProbability = true;
            }
            break;
          }
        }
      }

      state.airTemp = doc["current"]["temperature_2m"] | state.airTemp;
      state.airHum = doc["current"]["relative_humidity_2m"] | state.airHum;
      if (gotProbability) {
        state.lastWeatherSuccess = millis();
        state.weatherValid = true;
        Serial.printf("[Weather] API mua=%d%% T=%.1f H=%d\n",
                      state.rainApi, state.airTemp, state.airHum);
      } else {
        Serial.println("[Weather] Khong doc duoc xac suat mua theo gio");
      }
    } else {
      Serial.println("[Weather] JSON khong hop le");
    }
  } else {
    Serial.printf("[Weather] HTTP loi %d\n", code);
  }

  http.end();
}
}  // namespace

void beginWeatherService(AppState& state) {
  state.lastWeatherCheck = millis() - 600000UL;
}

void updateWeather(AppState& state, unsigned long now) {
  if (now - state.lastWeatherCheck >= 600000UL) {
    state.lastWeatherCheck = now;
    fetchWeather(state);
  }

  if (state.weatherValid && now - state.lastWeatherSuccess >= WEATHER_MAX_AGE_MS) {
    state.weatherValid = false;
    Serial.println("[Weather] Forecast het han; fallback sang cam bien cuc bo");
  }
}

