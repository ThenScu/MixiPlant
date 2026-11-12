// Copy to include/secrets.h (already git-ignored) and replace values locally.
// Never commit the real file, passwords, or private keys.
#define MIXIPLAN_WIFI_SSID "YOUR_WIFI_NAME"
#define MIXIPLAN_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Demo mode: leave TLS disabled and use only with non-critical Wokwi tests.
// Production: set the actual broker hostname, TLS port, CA, and credentials.
#define MIXIPLAN_MQTT_HOST "YOUR_MQTT_BROKER_HOST"
#define MIXIPLAN_MQTT_PORT 8883
#define MIXIPLAN_MQTT_TLS 1
#define MIXIPLAN_MQTT_USERNAME "YOUR_DEVICE_USERNAME"
#define MIXIPLAN_MQTT_PASSWORD "YOUR_DEVICE_PASSWORD"
#define MIXIPLAN_MQTT_CA_CERT "-----BEGIN CERTIFICATE-----\nPASTE_CA_CERTIFICATE\n-----END CERTIFICATE-----\n"

// AWS IoT Core mutual TLS requires the device certificate and private key too.
#define MIXIPLAN_MQTT_CLIENT_CERT "-----BEGIN CERTIFICATE-----\nPASTE_DEVICE_CERTIFICATE\n-----END CERTIFICATE-----\n"
#define MIXIPLAN_MQTT_PRIVATE_KEY "-----BEGIN PRIVATE KEY-----\nPASTE_PRIVATE_KEY\n-----END PRIVATE KEY-----\n"
