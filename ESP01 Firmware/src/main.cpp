#include <Arduino.h>
#include <OTADash.h>
#include <MQTTRelay.h>

#define RELAY_EEPROM_ADDR           100
#define RELAY_EEPROM_SIZE           50

OTADash       *otaDash            = nullptr;
MQTTRelay     *mqttRelay          = nullptr;
bool          otaModeActive       = false;
bool          relayState          = false;

ChronoLogger  mainLogger("Main", CHRONOLOG_LEVEL_DEBUG);

void connectToWiFi();
bool readCredentials();
bool checkButtonPress();
void initializeOTAMode();
void initializeMQTTRelay();
String generateDeviceUUID();
String generateDeviceName(const String& uuid);

void setup() {
  Serial.begin(115200);
  Serial.println();
  
  // Initialize random seed for fallback UUID generation
  randomSeed(analogRead(0) + ESP.getCycleCount());
  
  pinMode(LED_BUILTIN, INPUT_PULLUP);

  // Check credentials and button press to determine mode
  bool hasCredentials = readCredentials();
  bool buttonPressed = hasCredentials ? checkButtonPress() : false;
  
  if (!hasCredentials || buttonPressed) {
    const char* reason = !hasCredentials ? "No WiFi credentials found" : "Button pressed";
    mainLogger.warn("%s. Starting in OTA mode.", reason);
    initializeOTAMode();
  } else {
    mainLogger.info("Normal mode - Starting MQTT Relay");
    pinMode(LED_BUILTIN, OUTPUT);
    connectToWiFi();
    
    // Initialize MQTT Relay if WiFi connected
    if (WiFi.status() == WL_CONNECTED) {
      initializeMQTTRelay();
    }
  }
}

void loop() {
  if (!otaModeActive) {
    // Handle MQTT relay operations
    if (mqttRelay) {
      mqttRelay->loop();
    }
  }
}

bool checkButtonPress() {
  mainLogger.info("Press and hold button for 3 seconds to enter OTA mode...");
  
  uint32_t buttonPressStart = 0;
  uint32_t checkStart = millis();
  
  // Check for 10 seconds if user wants OTA mode
  while (millis() - checkStart < 10000) {
    if (digitalRead(LED_BUILTIN) == LOW) {
      if (buttonPressStart == 0) {
        buttonPressStart = millis();
        mainLogger.info("Button pressed...");
      } else if (millis() - buttonPressStart >= 3000) {
        return true; // Button held for 3 seconds
      }
    } else {
      buttonPressStart = 0; // Reset if button released
    }
    delay(100);
  }
  return false;
}

void initializeOTAMode() {
  otaDash = new OTADash("Wasa_Controller", "", "wasa_controller", "Wasa_Controller Portal");
  otaDash->begin(NetworkMode::ACCESS_POINT);
  otaModeActive = true;
}

void initializeMQTTRelay() {
  mainLogger.info("Initializing MQTT Relay Controller...");
  
  // Generate unique device UUID and name from MAC address
  String deviceUUID = generateDeviceUUID();
  String deviceName = generateDeviceName(deviceUUID);
  
  mainLogger.info("Generated Device UUID: %s", deviceUUID.c_str());
  mainLogger.info("Generated Device Name: %s", deviceName.c_str());
  
  mqttRelay = new MQTTRelay(LED_BUILTIN, deviceUUID.c_str(), deviceName.c_str());
  
  if (mqttRelay->begin()) {
    mainLogger.info("MQTT Relay Controller initialized successfully");
    
    // Attempt initial connection
    if (mqttRelay->connect()) {
      mainLogger.info("Connected to MQTT broker");
    } else {
      mainLogger.warn("Failed initial MQTT connection - will retry automatically");
    }
  } else {
    mainLogger.error("Failed to initialize MQTT Relay Controller");
  }
}

void connectToWiFi() {
  NetworkCredentials creds = otaDash ? otaDash->getNetworkCredentials() : NetworkCredentials{};
  
  // If using OTADash, get credentials from it, otherwise read from EEPROM
  if (!otaDash) {
    EEPROM.begin(OTA_DASH_EEPROM_SIZE);
    EEPROM.get(OTA_DASH_EEPROM_ADDR, creds);
    EEPROM.end();
  }

  if (strlen(creds.ssid) == 0) {
    mainLogger.error("SSID is empty, cannot connect to WiFi");
    return;
  }

  mainLogger.info("Connecting to WiFi SSID: %s", creds.ssid);
  WiFi.begin(creds.ssid, creds.password);

  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    mainLogger.info("Attempting to connect...");
  }

  if (WiFi.status() == WL_CONNECTED) {
    mainLogger.info("Connected to WiFi! IP address: %s", WiFi.localIP().toString().c_str());
  } else {
    mainLogger.error("Failed to connect to WiFi after 10 seconds");
  }
}

bool readCredentials() {
  NetworkCredentials creds;
  EEPROM.begin(OTA_DASH_EEPROM_SIZE);
  EEPROM.get(OTA_DASH_EEPROM_ADDR, creds);
  EEPROM.end();

  bool hasValidCredentials = strcmp(creds.setuped, "true") == 0;
  if (!hasValidCredentials) {
    mainLogger.warn("No WiFi credentials found in EEPROM");
  }
  
  return hasValidCredentials;
}

bool readRelayState() {
  EEPROM.begin(RELAY_EEPROM_SIZE);
  EEPROM.get(RELAY_EEPROM_ADDR, relayState);
  EEPROM.end();
  return relayState; 
}

bool writeRelayState(bool state) {
  relayState = state;
  EEPROM.begin(RELAY_EEPROM_SIZE);
  EEPROM.put(RELAY_EEPROM_ADDR, relayState);
  bool success = EEPROM.commit();
  EEPROM.end();
  if (success) {
    mainLogger.info("Relay state saved to EEPROM: %s", relayState ? "ON" : "OFF");
  } else {
    mainLogger.error("Failed to save relay state to EEPROM");
  }
  return success;
}

String generateDeviceUUID() {
  String macAddr = WiFi.macAddress();
  
  // Check if MAC address is valid
  if (macAddr.length() == 0 || macAddr == "00:00:00:00:00:00") {
    mainLogger.warn("Invalid MAC address, using fallback UUID");
    return "ESP-UNKNOWN-" + String(random(0x1000, 0xFFFF), HEX);
  }
  
  // Remove colons and convert to uppercase for consistency
  macAddr.replace(":", "");
  macAddr.toUpperCase();
  
  String deviceUUID = "WASA-" + macAddr;
  
  return deviceUUID;
}

String generateDeviceName(const String& uuid) {
  String shortId = uuid.substring(uuid.length() - 6);
  String deviceName = "WASA_Relay_" + shortId;
  return deviceName;
}