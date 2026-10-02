#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Ra do that: pot1 (Wokwi) -> cam bien dat capacitive
#define SOIL_PIN    34
// Mua truc tiep: tam mua YL-83 (Wokwi: nut nhan + pot2)
#define RAIN_AO_PIN 35   // pot2 van test cuong do mua
#define RAIN_DO_PIN 27   // nut TROIMUA nhan = dang mua (LOW)
#define RELAY_PIN   26

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int CRITICAL_LOW    = 15; // <15% SINH TON: tuoi khan cap chap mua
const int DRY_THRESHOLD   = 40; // <40% DAT KHO: can tuoi
const int TARGET_MOISTURE = 65; // >=65% DU AM: tat
const int RAIN_LIGHT      = 30; // 30-69% MUA NHO: bo qua, tinh theo am dat
const int RAIN_THRESHOLD  = 70; // >=70% MUA TO: hoan tuoi
const unsigned long PUMP_MIN_SWITCH_MS = 10000;
const unsigned long RAIN_TEST_TIMEOUT   = 120000UL; // test mua 2 phut tu ve AUTO

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* MQTT_BROKER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* TOPIC_PUB   = "mixiplan/trangthai";
const char* TOPIC_SUB   = "mixiplan/bom/set"; // ON / OFF / AUTO / RAIN:0-100

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Trang thai
int soilBase = 0;        // % goc tu pot1 (dat that)
int soilRaw = 0;
float irrigBoost = 0;    // nuoc bom cong don -> de demo tu ON/OFF
int currentMoisture = 0; // % hien thi = base + boost
int rainApi = 20;        // du bao Open-Meteo
int rainPot = 0;         // pot2 van test
int rainEff = 20;        // mua dung de quyet dinh
bool rainingNow = false; // DO truc tiep
float airTemp = 0;
int airHum = 0;
bool pumpActive = false;
String statusText = "KHOI DONG";
String pumpMode = "AUTO";

int rainTestVal = -1;
unsigned long rainTestUntil = 0;
unsigned long lastWeatherCheck = 0;
unsigned long lastPumpChange = 0;
unsigned long lastMqttPub = 0;
unsigned long lastLcd = 0;
unsigned long lastSecond = 0;

int docTrungBinh(int pin) {
  long s = 0;
  for (int i = 0; i < 10; i++) { s += analogRead(pin); delay(2); }
  return s / 10;
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setTimeout(5000);
  http.begin("http://api.open-meteo.com/v1/forecast?latitude=10.82&longitude=106.63&current=temperature_2m,relative_humidity_2m,precipitation_probability");
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String p = http.getString();
    JsonDocument doc;
    if (!deserializeJson(doc, p)) {
      rainApi = doc["current"]["precipitation_probability"] | rainApi;
      airTemp = doc["current"]["temperature_2m"] | airTemp;
      airHum  = doc["current"]["relative_humidity_2m"] | airHum;
      Serial.printf("[Weather] API mua=%d%% T=%.1f H=%d\n", rainApi, airTemp, airHum);
    }
  } else {
    Serial.printf("[Weather] HTTP loi %d\n", code);
  }
  http.end();
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim(); msg.toUpperCase();
  Serial.printf("[MQTT] app gui: %s\n", msg.c_str());
  if (msg == "ON") pumpMode = "MANUAL_ON";
  else if (msg == "OFF") pumpMode = "MANUAL_OFF";
  else if (msg.startsWith("RAIN:")) {
    int v = msg.substring(5).toInt();
    rainTestVal = constrain(v, 0, 100);
    rainTestUntil = millis() + RAIN_TEST_TIMEOUT;
    Serial.printf("[Test] ep mua=%d%% trong 2 phut\n", rainTestVal);
  }
  else pumpMode = "AUTO";
}

void mqttPublish() {
  if (!mqtt.connected()) return;
  JsonDocument doc;
  doc["am_goc"] = soilBase;
  doc["am"] = currentMoisture;
  doc["mua_api"] = rainApi;
  doc["mua_pot"] = rainPot;
  doc["mua"] = rainEff;
  doc["troi_mua"] = rainingNow;
  doc["bom"] = pumpActive ? "ON" : "OFF";
  doc["chedo"] = statusText;
  String out;
  serializeJson(doc, out);
  mqtt.publish(TOPIC_PUB, out.c_str());
}

void mqttReconnect() {
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
  String id = "ESP32-MixiPlan-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (mqtt.connect(id.c_str())) {
    mqtt.subscribe(TOPIC_SUB);
    Serial.println("[MQTT] OK, da subscribe lenh");
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(RAIN_DO_PIN, INPUT_PULLUP); // nhan nut = LOW = dang mua

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ESP32 TUOI CAY");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) { delay(400); retry++; Serial.print("."); }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\n[WiFi] OK" : "\n[WiFi] FAIL-offline");

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  fetchWeather();
  lastWeatherCheck = millis();
}

void loop() {
  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED) WiFi.reconnect();
  mqttReconnect();
  mqtt.loop();

  if (now - lastWeatherCheck >= 600000UL) { lastWeatherCheck = now; fetchWeather(); }

  // 1. Doc cam bien goc
  soilRaw = docTrungBinh(SOIL_PIN);
  soilBase = constrain(map(soilRaw, 0, 4095, 0, 100), 0, 100);
  int rainRaw = docTrungBinh(RAIN_AO_PIN);
  rainPot = constrain(map(rainRaw, 0, 4095, 0, 100), 0, 100);
  rainingNow = (digitalRead(RAIN_DO_PIN) == LOW); // truc tiep

  // 2. Chon mua hieu luc truoc de vat ly dung ngay: test app (2 phut) > truc tiep > pot test > API
  if (rainTestVal >= 0 && now < rainTestUntil) {
    rainEff = rainTestVal;
  } else {
    rainTestVal = -1;
    if (rainingNow) rainEff = 95;                    // nut nhan = mua to
    else if (abs(rainPot - rainApi) > 15) rainEff = rainPot; // dang van pot2 test
    else rainEff = rainApi;                          // binh thuong theo du bao
  }

  // 3. Vat ly dat: bom + nuoc, mua + nuoc, nang - nuoc
  if (now - lastSecond >= 1000) {
    lastSecond = now;
    float delta = 0;
    if (pumpActive) delta += 2.0;   // bom tuoi
    else delta -= 1.0;              // nang boc hoi
    if (rainingNow) delta += 1.5;   // mua to truc tiep
    else if (rainEff >= RAIN_THRESHOLD) delta += 1.0; // mua to du bao
    else if (rainEff >= RAIN_LIGHT) delta += 0.3;     // mua nho tham it
    irrigBoost += delta;
    if (irrigBoost > 80) irrigBoost = 80;
    if (irrigBoost < -40) irrigBoost = -40;
  }
  currentMoisture = constrain((int)(soilBase + irrigBoost), 0, 100);

  // 4. Logic tuoi theo nguong ro rang
  // <15 SINH TON | 15-40 KHO (mua to nghi, mua nho theo am) | 40-65 DUY TRI | >=65 DU AM
  bool muonBat = pumpActive;
  String lydo = statusText;
  bool muaTo = rainingNow || (rainEff >= RAIN_THRESHOLD);
  // bool muaNho = !muaTo && (rainEff >= RAIN_LIGHT); // mua nho -> tinh theo am, khong can nhanh rieng
  if (pumpMode == "MANUAL_ON")       { muonBat = true;  lydo = "APP BAT"; }
  else if (pumpMode == "MANUAL_OFF") { muonBat = false; lydo = "APP TAT"; }
  else if (currentMoisture >= TARGET_MOISTURE) { muonBat = false; lydo = "DU AM"; }
  else if (currentMoisture < CRITICAL_LOW)     { muonBat = true;  lydo = "SINH TON"; }
  else if (currentMoisture < DRY_THRESHOLD) {
    // Dat kho: mua to thi nghi, mua nho/khong mua thi tuoi theo am
    if (muaTo) { muonBat = false; lydo = rainingNow ? "TROI MUA" : "CHO MUA"; }
    else if (rainEff >= RAIN_LIGHT) { muonBat = true; lydo = "MUA NHO"; }
    else { muonBat = true; lydo = "DANG TUOI"; }
  } else {
    // Vung duy tri 40-65%: mua to thi tat, mua nho/khong mua giu hysteresis
    if (muaTo) { muonBat = false; lydo = rainingNow ? "TROI MUA" : "CHO MUA"; }
    else if (rainEff >= RAIN_LIGHT) { lydo = pumpActive ? "MUA NHO" : "STANDBY"; muonBat = pumpActive; }
    else { lydo = pumpActive ? "DANG TUOI" : "STANDBY"; muonBat = pumpActive; }
  }

  if (muonBat != pumpActive && (now - lastPumpChange >= PUMP_MIN_SWITCH_MS)) {
    pumpActive = muonBat;
    lastPumpChange = now;
    statusText = lydo;
    Serial.printf("[Bom] -> %s (%s) am=%d(goc%d+boost%.0f) mua=%d\n",
      pumpActive ? "ON" : "OFF", statusText.c_str(), currentMoisture, soilBase, irrigBoost, rainEff);
  } else if (muonBat == pumpActive) {
    statusText = lydo;
  }

  digitalWrite(RELAY_PIN, pumpActive ? HIGH : LOW);

  if (now - lastLcd >= 500) {
    lastLcd = now;
    lcd.setCursor(0, 0);
    lcd.printf("Am:%2d%% Mua:%2d%%", currentMoisture, rainEff);
    lcd.setCursor(0, 1);
    lcd.printf("BOM:[%s] %-8s", pumpActive ? "ON " : "OFF", statusText.c_str());
    Serial.printf("Am=%d(goc%d) Mua=%d(api%d/pot%d)%s Bom=%s %s\n",
      currentMoisture, soilBase, rainEff, rainApi, rainPot,
      rainingNow ? "[DOI-mua]" : "", pumpActive ? "ON" : "OFF", statusText.c_str());
  }

  if (now - lastMqttPub >= 5000) { lastMqttPub = now; mqttPublish(); }
}
