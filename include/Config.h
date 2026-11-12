#pragma once

#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef MIXIPLAN_WIFI_SSID
#define MIXIPLAN_WIFI_SSID "Wokwi-GUEST"
#endif
#ifndef MIXIPLAN_WIFI_PASSWORD
#define MIXIPLAN_WIFI_PASSWORD ""
#endif
#ifndef MIXIPLAN_MQTT_HOST
#define MIXIPLAN_MQTT_HOST "broker.hivemq.com"
#endif
#ifndef MIXIPLAN_MQTT_PORT
#define MIXIPLAN_MQTT_PORT 1883
#endif
#ifndef MIXIPLAN_MQTT_TLS
#define MIXIPLAN_MQTT_TLS 0
#endif
#ifndef MIXIPLAN_MQTT_USERNAME
#define MIXIPLAN_MQTT_USERNAME ""
#endif
#ifndef MIXIPLAN_MQTT_PASSWORD
#define MIXIPLAN_MQTT_PASSWORD ""
#endif
#ifndef MIXIPLAN_MQTT_CA_CERT
#define MIXIPLAN_MQTT_CA_CERT ""
#endif
#ifndef MIXIPLAN_MQTT_CLIENT_CERT
#define MIXIPLAN_MQTT_CLIENT_CERT ""
#endif
#ifndef MIXIPLAN_MQTT_PRIVATE_KEY
#define MIXIPLAN_MQTT_PRIVATE_KEY ""
#endif

// Chân đang dùng trong diagram.json (ESP32 DevKit v1).
constexpr uint8_t SOIL_PIN = 34;
constexpr uint8_t RAIN_AO_PIN = 35;
constexpr uint8_t RAIN_DO_PIN = 27;
constexpr uint8_t RELAY_PIN = 26;
constexpr uint8_t VALVE_RELAY_PIN = 25;
constexpr uint8_t LCD_SDA_PIN = 21;
constexpr uint8_t LCD_SCL_PIN = 22;

// Hieu chuan mac dinh cho potentiometer Wokwi; can do lai voi cam bien that.
constexpr int SOIL_RAW_DRY = 0;
constexpr int SOIL_RAW_WET = 4095;

// Bat ON cho Wokwi; tat khi dung cam bien dat that de lay so do truc tiep.
constexpr bool SIMULATE_SOIL_RESPONSE = true;
// Deep Sleep danh cho ban phan cung: mac dinh tat de simulator/MQTT van cap nhat lien tuc.
constexpr bool ENABLE_LOW_POWER_DEEP_SLEEP = false;
constexpr unsigned long LOW_POWER_WAKE_INTERVAL_MS = 300000UL;

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

static const char* const WIFI_SSID = MIXIPLAN_WIFI_SSID;
static const char* const WIFI_PASSWORD = MIXIPLAN_WIFI_PASSWORD;
static const char* const MQTT_BROKER = MIXIPLAN_MQTT_HOST;
constexpr uint16_t MQTT_PORT = MIXIPLAN_MQTT_PORT;
constexpr bool MQTT_USE_TLS = MIXIPLAN_MQTT_TLS != 0;
static const char* const MQTT_USERNAME = MIXIPLAN_MQTT_USERNAME;
static const char* const MQTT_PASSWORD = MIXIPLAN_MQTT_PASSWORD;
static const char* const MQTT_CA_CERT = MIXIPLAN_MQTT_CA_CERT;
static const char* const MQTT_CLIENT_CERT = MIXIPLAN_MQTT_CLIENT_CERT;
static const char* const MQTT_PRIVATE_KEY = MIXIPLAN_MQTT_PRIVATE_KEY;
// Topic prefix riêng để giảm va chạm với các thiết bị dùng chung broker công cộng.
static const char* const TOPIC_PUB = "mixiplan/tuoicay_mx7k2/trangthai";
static const char* const TOPIC_SUB = "mixiplan/tuoicay_mx7k2/bom/set";
static const char* const TOPIC_ACK = "mixiplan/tuoicay_mx7k2/bom/ack";

