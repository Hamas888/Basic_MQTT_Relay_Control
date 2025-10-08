#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

#include <Arduino.h>

// MQTT Broker Configuration
#define MQTT_BROKER_HOST        "192.168.0.109"
#define MQTT_BROKER_PORT        8883
#ifndef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE          60
#endif
#define MQTT_QOS                1
#define MQTT_RETAINED           true

// Device Configuration (Note: UUID and Name are now generated dynamically from MAC address)
// Legacy defines kept for compatibility - actual values generated at runtime
#define DEVICE_UUID             "AUTO_GENERATED"  // Generated from MAC address
#define DEVICE_NAME             "AUTO_GENERATED"  // Generated from MAC address

// Topic Configuration
#define UPLINK_TOPIC_PREFIX     "ControlDevice/Uplink/"
#define DOWNLINK_TOPIC_PREFIX   "ControlDevice/Downlink/"
#define STATUS_TOPIC_PREFIX     "ControlDevice/Status/"

// Connection Settings
#define MQTT_RECONNECT_DELAY    5000    // 5 seconds
#define MQTT_MAX_RECONNECT_ATTEMPTS 10
#define HEARTBEAT_INTERVAL      30000   // 30 seconds

// EEPROM Settings
#define MQTT_EEPROM_ADDR        200
#define MQTT_EEPROM_SIZE        100

// Root CA Certificate (replace with your actual certificate)
extern const char* rootCACertificate;

// Client Certificate (replace with your actual certificate)  
extern const char* clientCertificate;

// Client Private Key (replace with your actual private key)
extern const char* clientPrivateKey;

// MQTT Configuration Structure
struct MQTTConfig {
    char brokerHost[64];
    int  brokerPort;
    char deviceUUID[32];
    char deviceName[64];
    bool useSSL;
    bool initialized;
};

#endif // MQTT_CONFIG_H