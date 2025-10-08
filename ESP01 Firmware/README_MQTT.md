# MQTT Relay Controller

This project implements an MQTT-based relay controller for ESP32/ESP8266 devices, mimicking the functionality of the provided Python script.

## Features

- **Secure MQTT Connection**: TLS/SSL support with client certificates
- **Relay Control**: Remote relay on/off control via MQTT commands
- **State Persistence**: Relay state saved to EEPROM and restored on boot
- **Auto-Reconnection**: Automatic MQTT reconnection with configurable attempts
- **Heartbeat**: Periodic status updates
- **Last Will Testament**: Automatic offline status on unexpected disconnection
- **OTA Mode**: Configuration portal for WiFi setup

## MQTT Topics

The device uses the following topic structure:

- **Uplink** (Device → Server): `ControlDevice/Uplink/{DEVICE_UUID}`
- **Downlink** (Server → Device): `ControlDevice/Downlink/{DEVICE_UUID}`
- **Status**: `ControlDevice/Status/{DEVICE_UUID}`

## Message Formats

### Commands (Downlink)

**Set Relay State:**
```json
{
  "command": "set_state",
  "state": "on"  // or "off"
}
```

**Get Relay State:**
```json
{
  "command": "get_state"
}
```

### Responses (Uplink)

**Hello Message:**
```json
{
  "device_uuid": "ESP32-AABBCCDDEEFF",
  "message": "hello",
  "device_name": "ESP32_Relay_Controller",
  "state": "off",
  "timestamp": "1234567890"
}
```

**Acknowledgment:**
```json
{
  "device_uuid": "ESP32-AABBCCDDEEFF",
  "command": "ack",
  "original_command": "set_state",
  "success": true,
  "state": "on",
  "timestamp": "1234567890"
}
```

**Heartbeat:**
```json
{
  "device_uuid": "ESP32-AABBCCDDEEFF",
  "message": "heartbeat",
  "state": "on",
  "uptime": 1234567,
  "timestamp": "1234567890"
}
```

**Status:**
```json
{
  "device_uuid": "ESP32-AABBCCDDEEFF",
  "status": "online",  // or "offline"
  "timestamp": "1234567890"
}
```

## Configuration

### Hardware Setup
- Connect relay to GPIO pin 2 (configurable in main.cpp)
- Built-in LED indicates connection status:
  - Slow blink: MQTT connected
  - Fast blink: MQTT disconnected

### MQTT Configuration
Update `include/MQTTConfig.h`:

1. **Broker Settings:**
   ```cpp
   #define MQTT_BROKER_HOST        "your-mqtt-broker.com"
   #define MQTT_BROKER_PORT        8883
   ```

2. **Device Settings:**
   ```cpp
   #define DEVICE_UUID             "YOUR_UNIQUE_ID"
   #define DEVICE_NAME             "Your_Device_Name"
   ```

3. **Certificates:**
   Replace the certificate strings with your actual certificates:
   - `rootCACertificate`: Root CA certificate
   - `clientCertificate`: Client certificate (optional)
   - `clientPrivateKey`: Client private key (optional)

## Usage

### Basic Usage
```cpp
#include <MQTTRelay.h>

#define RELAY_PIN 2

MQTTRelay mqttRelay(RELAY_PIN);

void setup() {
    // Initialize WiFi first
    WiFi.begin("ssid", "password");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
    }
    
    // Initialize MQTT Relay
    mqttRelay.begin();
    mqttRelay.connect();
}

void loop() {
    mqttRelay.loop();
}
```

### Advanced Configuration
```cpp
// Set custom broker
mqttRelay.setBrokerConfig("custom-broker.com", 8883, true);

// Manual relay control
mqttRelay.setRelayState(true);  // Turn on
mqttRelay.setRelayState(false); // Turn off

// Check connection status
if (mqttRelay.isConnected()) {
    // Send custom heartbeat
    mqttRelay.sendHeartbeat();
}
```

## LED Status Indicators

- **Slow blink (2s cycle)**: MQTT connected and operational
- **Fast blink (0.5s cycle)**: MQTT disconnected, attempting reconnection
- **Solid on**: OTA mode active

## OTA Configuration Mode

Hold the button for 3 seconds during startup to enter OTA mode:
1. Device creates WiFi access point "Wasa_Controller"
2. Connect to AP and navigate to configuration portal
3. Set WiFi credentials and other settings
4. Device restarts in normal mode

## Troubleshooting

### Common Issues

1. **MQTT Connection Failed:**
   - Check broker hostname and port
   - Verify certificates if using SSL
   - Check network connectivity

2. **Relay Not Responding:**
   - Verify GPIO pin configuration
   - Check relay wiring
   - Monitor serial output for errors

3. **State Not Persisting:**
   - EEPROM initialization may have failed
   - Check EEPROM size configuration

### Debug Output
Enable debug logging to see detailed operation:
```cpp
ChronoLogger logger("Debug", CHRONOLOG_LEVEL_DEBUG);
```

## Memory Usage

- **EEPROM**: ~100 bytes for MQTT configuration and relay state
- **RAM**: ~8KB for MQTT client and buffers
- **Flash**: ~50KB additional code size

## Security Notes

1. **Certificate Validation**: Remove `setInsecure()` in production
2. **Unique Device IDs**: Use MAC address or secure random generation
3. **Credential Storage**: Consider encryption for stored credentials
4. **Network Security**: Use WPA2/WPA3 for WiFi networks

## Compatibility

- **ESP32**: Full feature support
- **ESP8266**: Full feature support (using ESP8266WiFi library)
- **Arduino IDE**: Supported with library dependencies
- **PlatformIO**: Recommended development environment