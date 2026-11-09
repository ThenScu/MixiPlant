#pragma once

#include <Arduino.h>

// Chân đang dùng trong diagram.json (ESP32 DevKit v1).
constexpr uint8_t SOIL_PIN = 34;
constexpr uint8_t RAIN_AO_PIN = 35;
constexpr uint8_t NPK_PIN = 32;
constexpr uint8_t EC_PIN = 33;
constexpr uint8_t RAIN_DO_PIN = 27;
constexpr uint8_t RELAY_PIN = 26;
constexpr uint8_t VALVE_RELAY_PIN = 25;
constexpr uint8_t LCD_SDA_PIN = 21;
constexpr uint8_t LCD_SCL_PIN = 22;

// Hieu chuan mac dinh cho potentiometer Wokwi; can do lai voi cam bien that.
constexpr int SOIL_RAW_DRY = 0;
constexpr int SOIL_RAW_WET = 4095;

// Nguong dieu khien demo. 40% la nguong kho bat dau chu ky, 65% la nguong dung.
constexpr int CRITICAL_LOW = 15;
constexpr int DRY_THRESHOLD = 40;
constexpr int TARGET_MOISTURE = 65;
constexpr int RAIN_LIGHT = 30;
constexpr int RAIN_THRESHOLD = 70;

constexpr unsigned long PUMP_MIN_SWITCH_MS = 10000UL;
constexpr unsigned long RAIN_TEST_TIMEOUT_MS = 120000UL;
constexpr unsigned long NORMAL_PULSE_MS = 120000UL;
constexpr unsigned long SURVIVAL_PULSE_MS = 30000UL;
constexpr unsigned long PULSE_REST_MS = 180000UL;

constexpr unsigned long WIFI_RETRY_MS = 10000UL;
constexpr unsigned long MQTT_RETRY_MIN_MS = 5000UL;
constexpr unsigned long MQTT_RETRY_MAX_MS = 60000UL;
constexpr unsigned long WEATHER_MAX_AGE_MS = 900000UL;

static const char* const WIFI_SSID = "Wokwi-GUEST";
static const char* const WIFI_PASSWORD = "";
static const char* const MQTT_BROKER = "broker.hivemq.com";
constexpr uint16_t MQTT_PORT = 1883;
static const char* const TOPIC_PUB = "mixiplan/trangthai";
static const char* const TOPIC_SUB = "mixiplan/bom/set";

