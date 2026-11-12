#include "Display.h"

#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>

#include "Config.h"

namespace {
LiquidCrystal_I2C lcdMain(0x27, 16, 2);
LiquidCrystal_I2C lcdSensors(0x26, 16, 2);

void writeLine(LiquidCrystal_I2C& lcd, uint8_t row, const char* text) {
  char line[17];
  memset(line, ' ', 16);
  line[16] = '\0';
  const size_t length = min(strlen(text), static_cast<size_t>(16));
  memcpy(line, text, length);
  lcd.setCursor(0, row);
  lcd.print(line);
}
}

void beginDisplay() {
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcdMain.init();
  lcdSensors.init();
  lcdMain.backlight();
  lcdSensors.backlight();
  writeLine(lcdMain, 0, "MIXIPLAN CHINH");
  writeLine(lcdMain, 1, "Khoi dong...");
  writeLine(lcdSensors, 0, "CAM BIEN DEMO");
  writeLine(lcdSensors, 1, "Khoi dong...");
}

void updateDisplay(AppState& state, unsigned long now, bool wifiConnected) {
  if (now - state.lastLcd < 500UL) return;
  state.lastLcd = now;

  char mainLine1[32];
  char mainLine2[32];
  char sensorLine1[32];
  char sensorLine2[32];
  char apiRain[5];
  if (state.weatherValid) {
    snprintf(apiRain, sizeof(apiRain), "%d", state.rainApi);
  } else {
    strcpy(apiRain, "--");
  }
  snprintf(mainLine1, sizeof(mainLine1), "AM:%3d%% M:%3d%%",
           state.currentMoisture, state.rainEff);
  snprintf(mainLine2, sizeof(mainLine2), "BOM:%s VAN:%s",
           state.pumpActive ? "ON" : "OFF",
           state.pumpActive ? "MO" : "DONG");
  snprintf(sensorLine1, sizeof(sensorLine1), "T:%4.1fC H:%2d%%",
           state.airTemp, state.airHum);
  snprintf(sensorLine2, sizeof(sensorLine2), "API:%s M:%s",
           apiRain,
           state.mqttOnline ? "ON" : "OFF");

  writeLine(lcdMain, 0, mainLine1);
  writeLine(lcdMain, 1, mainLine2);
  writeLine(lcdSensors, 0, sensorLine1);
  writeLine(lcdSensors, 1, sensorLine2);

  Serial.printf("Am=%d (ADC=%d) Mua=%d (API=%d/pot=%d) T=%.1f H=%d, bom=%s, %s%s\n",
                state.currentMoisture, state.soilRaw, state.rainEff,
                state.rainApi, state.rainPot, state.airTemp, state.airHum,
                state.pumpActive ? "ON" : "OFF",
                state.statusText.c_str(), wifiConnected ? "" : " [OFFLINE EDGE]");
}
