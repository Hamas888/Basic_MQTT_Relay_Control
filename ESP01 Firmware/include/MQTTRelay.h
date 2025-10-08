#ifndef MQTT_RELAY_H
#define MQTT_RELAY_H

#include <Arduino.h>
#ifdef ESP8266
    #include <ESP8266WiFi.h>
    #include <WiFiClientSecure.h>
    #include <WiFiClientSecureBearSSL.h>
#else
    #include <WiFi.h>
    #include <WiFiClientSecure.h>
#endif
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ChronoLog.h>
#include "MQTTConfig.h"

class MQTTRelay {
    public:

        MQTTRelay(uint8_t relayPin, const char* deviceUUID = DEVICE_UUID, const char* deviceName = DEVICE_NAME);
        ~MQTTRelay();
        void loop();
        bool begin();
        bool connect();
        void disconnect();
        bool isConnected();

        void sendHeartbeat();
        bool setRelayState(bool state, bool saveToEEPROM = true);
        void sendStatus(const char* status, bool retained = true);

        bool getRelayState() const { return relayState; }
        void setBrokerConfig(const char* host, int port, bool useSSL = true);

    private:
        int                 reconnectAttempts;
        bool                autoReconnect;
        bool                relayState;
        uint8_t             relayPin;
        String              deviceUUID;
        String              deviceName;
        String              uplinkTopic;
        String              downlinkTopic;
        String              statusTopic;
        MQTTConfig          config;
        WiFiClient          wifiClient;
        ChronoLogger        logger;
        unsigned long       lastHeartbeat;
        unsigned long       lastReconnectAttempt;
        PubSubClient*       mqttClient;
        WiFiClientSecure    wifiClientSecure;

        void setupSSL();
        static void mqttCallback(char* topic, byte* payload, unsigned int length);
        static MQTTRelay* instance;
        void handleMessage(char* topic, byte* payload, unsigned int length);
        void processCommand(JsonDocument& command);
        void sendAck(const char* command, bool success, const char* state = nullptr);
        void sendHello();
        bool loadConfig();
        bool saveConfig();
        bool loadRelayState();
        bool saveRelayState();
        String getCurrentTimestamp();
        void handleReconnection();
        void setupLastWill();
};

#endif // MQTT_RELAY_H