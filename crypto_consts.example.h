#ifndef CRYPTO_CONSTS_EXAMPLE_H_
#define CRYPTO_CONSTS_EXAMPLE_H_

// WiFi Credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// MQTT Credentials
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define MQTT_SERVER_HOST ""
#define MQTT_SERVER_PORT 8883

// Hive MQTT Broker
#ifdef HIVE_MQTT_BROKER
#define CRYPTO_CA \
"-----BEGIN CERTIFICATE-----\n" \
"\n" \
"-----END CERTIFICATE-----"
#endif

#endif
