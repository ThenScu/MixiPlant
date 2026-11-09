#include "Display.h"

#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "Config.h"

namespace {
LiquidCrystal_I2C lcd(0x27, 16, 2);
}

void beginDisplay() {
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 TUOI CAY");
  lcd.setCursor(0, 1);
  lcd.print("Khoi dong...");
}

void updateDisplay(AppState& state, unsigned long now, bool wifiConnected) {
  if (now - state.lastLcd < 500UL) return;
  state.lastLcd = now;

  lcd.setCursor(0, 0);
  lcd.printf("Am:%2d%% Mua:%2d%%", state.currentMoisture, state.rainEff);
  lcd.setCursor(0, 1);
  lcd.printf("BOM:[%s] %-8s", state.pumpActive ? "ON " : "OFF", state.statusText.c_str());

  Serial.printf("Am=%d (ADC=%d) Mua=%d (API=%d/pot=%d) NPK~%d EC~%d, bom=%s, %s%s\n",
                state.currentMoisture, state.soilRaw, state.rainEff,
                state.rainApi, state.rainPot, state.npkDemoMgKg,
                state.ecDemoUsCm, state.pumpActive ? "ON" : "OFF",
                state.statusText.c_str(), wifiConnected ? "" : " [OFFLINE EDGE]");
}

