   /*
 ====================================================================================================
 * File:        OTADash.cpp
 * Author:      Hamas Saeed
 * Version:     Rev_1.0.0
 * Date:        Feb 10 2025
 * Brief:       This Package Provide Wireless Intrective Features For IoT Devices Based On ESP32
 * 
 ====================================================================================================
 * License: 
 * MIT License
 * 
 * Copyright (c) 2025 Hamas Saeed
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * For any inquiries, contact Hamas Saeed at hamasaeed@gmail.com
 *
 ====================================================================================================
 */

#include "OTADash.h"

OTADash* OTADash::instance = nullptr;

OTADash::OTADash(const char* ssid, const char* password, const char* custom_domain, const char* portal_title) : 
    customDomain        (String(custom_domain) + ".local"), 
    ssid                (ssid), 
    password            (password), 
    portal_title        (portal_title),
    dnsServer           (std::make_unique<DNSServer>()),
    server              (std::make_unique<AsyncWebServer>(80)),
    ws                  (std::make_unique<AsyncWebSocket>("/ws")) {
    instance = this;

    #if OTA_DASH_ENABLE_DEBUG_LOGS
        if (!otaLogger) {
            otaLogger = new ChronoLogger("OTADash", CHRONOLOG_LEVEL_DEBUG);
        }
    #endif
}

OTADash::~OTADash() {
    #if defined(OTA_DASH_PLATFORM_ESP32)
        try {
            stop();
            if (instance == this) {
                instance = nullptr;
            }
        } catch (const std::exception& e) {                                                                             // Handle any exceptions that occur during server shutdown
            otaLogger->error("Error during server shutdown: {}", e.what());
        }
    #elif defined(OTA_DASH_PLATFORM_ESP8266)
        stop();
        if (instance == this) {
            instance = nullptr;
        }
    #endif
}

void OTADash::stop() {
    #if defined(OTA_DASH_PLATFORM_ESP8266)   
        clientHandlerTicker.detach();
    #endif 
    if (serverStarted) {
        ws->closeAll();
        server->end();
        dnsServer->stop();
        WiFi.softAPdisconnect(true);
        serverStarted = false;
        Serial.println("Server stopped");
    }
}

bool OTADash::readEEPROM() {
    EEPROM.begin(eepromSize);
    EEPROM.get(eepromAddress, networkCredentials);

    if(strcmp(networkCredentials.setuped, "true") != 0) {
        EEPROM.end();
        return false;
    }

    EEPROM.end();
    return true;
}

bool OTADash::writeEEPROM() {
    EEPROM.begin(eepromSize);
    EEPROM.put(eepromAddress, networkCredentials);
    bool success = EEPROM.commit();
    EEPROM.end();
    return success;
}

void OTADash::handleClient() {
    if (currentMode == NetworkMode::ACCESS_POINT || currentMode == NetworkMode::DUAL) {
        dnsServer->processNextRequest();
    }
    
    if ((currentMode == NetworkMode::STATION || currentMode == NetworkMode::DUAL) 
        && WiFi.status() != WL_CONNECTED && autoReconnect) {
        handleNetworkFailure();
    }
    
    int scanResult = WiFi.scanComplete();
    if (scanResult >= 0) {
        handleWifiScanResult(scanResult);
    }

    if(pairRequest) {
        handlePairingResult();
    }
}

void OTADash::begin(NetworkMode mode) {
    currentMode = mode;
    server->reset();
    otaLogger->debug("Starting server....");

    if(mode == NetworkMode::STATION || mode == NetworkMode::AUTO || mode == NetworkMode::DUAL) {
        if(!readEEPROM()) {
            otaLogger->warn("Wifi credentials not found. Mode set to ACCESS_POINT");
            currentMode = NetworkMode::ACCESS_POINT;                                                                // Default to Access Point mode if no credentials are found
        } else if(mode == NetworkMode::AUTO) {
            otaLogger->debug("Auto mode detected. Switching to STATION mode");
            currentMode = NetworkMode::STATION;                                                                     // Switch to Station mode if AUTO is selected
        }
    }
  
    switch (currentMode) {
        case NetworkMode::ACCESS_POINT:
            otaLogger->debug("Access Point mode");
            if (!startAccessPoint()) {
                otaLogger->warn("Failed to start AP mode");
                return;
            }
            break;
            
        case NetworkMode::STATION:
            otaLogger->debug("Station mode");
            if (!startStation()) {
                otaLogger->warn("Failed to start STA mode");
                return;
            }
            break;

        case NetworkMode::DUAL:
            otaLogger->debug("Dual AP/STA mode");
            if(!startDualMode()) {
                otaLogger->warn("Failed to start Dual mode");
                return;
            }
            break;
        default:
            otaLogger->warn("Unknown network mode, defaulting to AUTO");
            currentMode = NetworkMode::AUTO;
            begin(NetworkMode::AUTO);  // Recursive call with AUTO
            return;
    }

    if (currentMode == NetworkMode::ACCESS_POINT || currentMode == NetworkMode::DUAL) {
        dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());
    }

    ws->onEvent([this](
        AsyncWebSocket *server, 
        AsyncWebSocketClient *client,
        AwsEventType type, 
        void *arg, 
        uint8_t *data, 
        size_t len) {

        #if defined(OTA_DASH_PLATFORM_ESP32)
            try {
                if (type == WS_EVT_DATA) {
                    AwsFrameInfo *info = (AwsFrameInfo *)arg;
                    if (info->opcode == WS_TEXT) {
                        data[len] = 0;
                        handleWebSocketMessage(arg, data, len);
                    }
                }
            } catch (const std::exception &e) {
                otaLogger->error("WebSocket error: {}", e.what());
            }
        #elif defined(OTA_DASH_PLATFORM_ESP8266)
            if (type == WS_EVT_DATA) {
                AwsFrameInfo *info = (AwsFrameInfo *)arg;
                if (info->opcode == WS_TEXT) {
                    data[len] = 0;
                    handleWebSocketMessage(arg, data, len);
                }
            }
        #endif
    });

    server->addHandler(ws.get());
    setupServer();
    server->begin();
    otaLogger->debug("Server started");

    if (currentMode == NetworkMode::STATION || currentMode == NetworkMode::DUAL) {
        otaLogger->debug(String("Station IP: " + WiFi.localIP().toString()).c_str());
        otaLogger->debug(String("Access in the browser by: http://" + WiFi.localIP().toString()).c_str());
    }
    if (currentMode == NetworkMode::ACCESS_POINT || currentMode == NetworkMode::DUAL) {
        otaLogger->debug(String("Access point IP: " + WiFi.softAPIP().toString()).c_str());
        otaLogger->debug(String("Access in the browser by: http://" + WiFi.softAPIP().toString()).c_str());
        otaLogger->debug(String("Access in the browser by: http://" + customDomain).c_str());
    }
    serverStarted = true;

    #if defined(OTA_DASH_PLATFORM_ESP32)
        xTaskCreatePinnedToCore(otaDashTask, "otaDashTask", 4096, instance, 1, &otaTaskHandle, 0);
    #elif defined(OTA_DASH_PLATFORM_ESP8266)
        clientHandlerTicker.attach_ms(100, [this]() { handleClientTick(); });
        
    #endif
}

void OTADash::printDebug(const String& message) {
    if (serverStarted && isOnDebugPage) {                                                                           // Replace escape sequences with HTML equivalents
        
        String formattedMessage = message;
        formattedMessage.replace("\n", "<br/>");
        formattedMessage.replace("\r", "");
        formattedMessage.replace("\t", "&emsp;");

        debugLogs += formattedMessage + "<br/>";
        debugLogsCounter++;
        
        #if defined(OTA_DASH_PLATFORM_ESP32)
        try {
            ws->textAll(formattedMessage);                                                                          // Send the formatted message to all WebSocket clients
        } catch (const std::exception& e) {
            otaLogger->error("Debug print error: {}", e.what());
        }
        #elif defined(OTA_DASH_PLATFORM_ESP8266)
            ws->textAll(formattedMessage);                                                                          // Send the formatted message to all WebSocket clients
        #endif

        if (debugLogsCounter >= debugLogsMax) {
            debugLogs = "";
            debugLogsCounter = 0;
        }
    }
}

String OTADash::encryptionTypeToString(int encryptionType) {                                                                                              // Helper function to convert encryption type to a string
    #if defined(OTA_DASH_PLATFORM_ESP32)
    switch (encryptionType) {
        case WIFI_AUTH_OPEN: return "Open";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        default: return "Unknown";
    }
    #elif defined(OTA_DASH_PLATFORM_ESP8266)
        switch (encryptionType) {
            case ENC_TYPE_WEP:  return "WEP";
            case ENC_TYPE_TKIP: return "WPA";
            case ENC_TYPE_CCMP: return "WPA2";
            case ENC_TYPE_NONE: return "Open";
            case ENC_TYPE_AUTO: return "Auto";
            default: return "Unknown";
        }
    #endif
}

void OTADash::disconnectWifi() {
    if (currentMode == NetworkMode::STATION || currentMode == NetworkMode::DUAL) {
        WiFi.disconnect();
    }
}

bool OTADash::connectToWifi(const char* ssid, const char* password, uint32_t timeout_ms) {
    if (currentMode == NetworkMode::ACCESS_POINT) {
        otaLogger->warn("Cannot connect to WiFi in AP-only mode");
        return false;
    }

    WiFi.begin(ssid, password);
    
    uint32_t startTime = millis();
    otaLogger->debug("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime >= timeout_ms) {
            otaLogger->warn("WiFi connection timeout");
            return false;
        }
        delay(500);
        otaLogger->debug(String("Trying to connect " + String(networkCredentials.ssid)).c_str());
    }

    otaLogger->debug("Connected to WiFi");
    return true;
}

void OTADash::setupServer() {
    server->onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            AsyncWebServerResponse *response = request->beginResponse(204);
            response->addHeader("Access-Control-Allow-Origin", "*");
            response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            response->addHeader("Access-Control-Allow-Headers", "Content-Type");
            request->send(response);
            return;
        }
        request->send(404, "text/plain", "Not Found");
    });

    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        isOnDebugPage = false;
        String html = index_html;
        html.replace("%PORTAL_HEADING%", portal_title);
        html.replace("%CUSTOM_DOMAIN%", customDomain);
        request->send(200, "text/html", html.c_str());
    });

    server->on("/info", HTTP_GET, [this](AsyncWebServerRequest *request){
        String infoHtml = device_info_html;
        String deviceInfo;
        deviceInfo =  "<tr><td>Product Name</td><td>"               + productName                                       + "</td></tr>";
        deviceInfo += "<tr><td>Firmware Version</td><td>"           + firmwareVersion                                   + "</td></tr>";
        #if defined(OTA_DASH_PLATFORM_ESP32)
            deviceInfo +=  "<tr><td>Chip Model</td><td>"            + String(ESP.getChipModel())                        + "</td></tr>";
            deviceInfo += "<tr><td>Chip Cores</td><td>"             + String(ESP.getChipCores())                        + "</td></tr>";
            deviceInfo += "<tr><td>Chip Revision</td><td>"          + String(ESP.getChipRevision())                     + "</td></tr>";
            deviceInfo += "<tr><td>CPU Frequency</td><td>"          + String(ESP.getCpuFreqMHz())                       + " MHz</td></tr>";
            deviceInfo += "<tr><td>Chip Temperature</td><td>"       + String(temperatureRead())                         + " °C</td></tr>";
        #elif defined(OTA_DASH_PLATFORM_ESP8266)
            deviceInfo +=  "<tr><td>Chip ID</td><td>"               + String(ESP.getChipId())                           + "</td></tr>";
            deviceInfo += "<tr><td>CPU Frequency</td><td>"          + String(ESP.getCpuFreqMHz())                       + " MHz</td></tr>";
        #endif
        deviceInfo += "<tr><td>Access Point SSID</td><td>"          + WiFi.softAPSSID()                                 + "</td></tr>";
        deviceInfo += "<tr><td>Access Point IP Address</td><td>"    + WiFi.softAPIP().toString()                        + "</td></tr>";
        deviceInfo += "<tr><td>Connected Clients</td><td>"          + String(WiFi.softAPgetStationNum())                + "</td></tr>";
        deviceInfo += "<tr><td>Flash Size</td><td>"                 + String(ESP.getFlashChipSize() / (1024 * 1024))    + " MB</td></tr>";
        deviceInfo += "<tr><td>Flash Speed</td><td>"                + String(ESP.getFlashChipSpeed() / 1000000)         + " MHz</td></tr>";
        deviceInfo += "<tr><td>Sketch Size</td><td>"                + String(ESP.getSketchSize() / (1024 * 1024))       + " MB</td></tr>";
        deviceInfo += "<tr><td>Free Sketch Space</td><td>"          + String(ESP.getFreeSketchSpace() / (1024 * 1024))  + " MB</td></tr>";
        #if defined(OTA_DASH_PLATFORM_ESP32)
            deviceInfo += "<tr><td>Heap Size</td><td>"              + String(ESP.getHeapSize() / (1024 * 1024))         + " MB</td></tr>";
            deviceInfo += "<tr><td>Free Heap</td><td>"              + String(ESP.getFreeHeap() / (1024 * 1024))         + " MB</td></tr>";
            deviceInfo += "<tr><td>PSRAM Size</td><td>"             + String(ESP.getPsramSize() / (1024 * 1024))        + " MB</td></tr>";
            deviceInfo += "<tr><td>Free PSRAM</td><td>"             + String(ESP.getFreePsram() / (1024 * 1024))        + " MB</td></tr>";
        #elif defined(OTA_DASH_PLATFORM_ESP8266)
            deviceInfo += "<tr><td>Free Heap</td><td>"              + String(ESP.getFreeHeap() / 1024)                  + " KB</td></tr>";
        #endif
        deviceInfo += "<tr><td>Uptime</td><td>"                     + String(millis() / 1000)                           + " seconds</td></tr>";

        infoHtml.replace("%DEVICE_INFO%", deviceInfo);
        request->send(200, "text/html", infoHtml.c_str());
    });

    server->on("/wifimanage", HTTP_GET, [this](AsyncWebServerRequest *request) {
        otaLogger->debug("Wi-Fi scan requested");
        #if defined(OTA_DASH_PLATFORM_ESP32)
            WiFi.scanNetworks(true);
        #elif defined(OTA_DASH_PLATFORM_ESP8266)
            WiFi.scanDelete();
            if (currentMode == NetworkMode::ACCESS_POINT) WiFi.mode(WIFI_AP_STA);
            WiFi.scanNetworks(true);
        #endif
        String html = wifi_manage_html;
        request->send(200, "text/html", html.c_str());
    });

    server->on("/save-wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String password = request->getParam("password", true)->value();

            if (ssid.isEmpty()) {
                request->send(400, "text/plain", "SSID cannot be empty");
                return;
            }
            if (password.length() < 8) {
                request->send(400, "text/plain", "Password must be at least 8 characters");
                return;
            }

            otaLogger->debug("WIFI Saved Request Received");

            if (wifiSavedCallback) {                                                                                // Call the user-defined callback if set
                wifiSavedCallback(ssid, password);
                request->send(200, "text/plain", "WiFi credentials saved");
            } else {
                otaLogger->debug(String("SSID: " + ssid).c_str());
                otaLogger->debug(String("Password: " + password).c_str());
                otaLogger->warn("Missing Saving Callback, Using Default Method");
                strcpy(networkCredentials.ssid, ssid.c_str());
                strcpy(networkCredentials.password, password.c_str());
                strcpy(networkCredentials.setuped, "true");
                writeEEPROM();
                request->send(200, "text/plain", "Missing Saving Callback");
                ESP.restart();
            }
            
        } else {
            request->send(400, "text/plain", "Missing SSID or password");
        }
    });

    server->on("/update", HTTP_GET, [this](AsyncWebServerRequest *request){
        String html = update_firmware_html;
        request->send(200, "text/html", html.c_str());
    });

    server->on("/update", HTTP_POST, [](AsyncWebServerRequest *request){
        handleUpdate(request);
    }, handleUpload);

    server->on("/erase", HTTP_GET, [this](AsyncWebServerRequest *request){
        String html = erase_settings_html;
        request->send(200, "text/html", html.c_str());
    });

    server->on("/erase", HTTP_POST, [this](AsyncWebServerRequest *request){
        strcpy(networkCredentials.ssid, "");
        strcpy(networkCredentials.password, "");
        strcpy(networkCredentials.setuped, "false");
        writeEEPROM();
        request->send(200, "text/html", "Settings erased.");
    });

    server->on("/debug", HTTP_GET, [this](AsyncWebServerRequest *request){
        String html = debug_html;
        isOnDebugPage = true;
        html.replace("%PORTAL_HEADING%", portal_title);
        request->send(200, "text/html", html.c_str());
    });

    server->on("/restart", HTTP_GET, [this](AsyncWebServerRequest *request){
        String html = restart_device_html;
        request->send(200, "text/html", html.c_str());
    });

    server->on("/restart", HTTP_POST, [this](AsyncWebServerRequest *request){
        request->send(200, "text/html", "Device is restarting...<br/>Please wait a moment.");
        #if defined(OTA_DASH_PLATFORM_ESP32)
            vTaskDelay(1000);
        #elif defined(OTA_DASH_PLATFORM_ESP8266)
            delay(1000);
        #endif
        ESP.restart();
    });

    server->on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest *request){
        String html = index_html;
        html.replace("%PORTAL_HEADING%", portal_title);
        html.replace("%CUSTOM_DOMAIN%", customDomain);
        request->send(200, "text/html", html.c_str());
    });

    server->on("/fwlink", HTTP_GET, [this](AsyncWebServerRequest *request){
        String html = index_html;
        html.replace("%PORTAL_HEADING%", portal_title);
        html.replace("%CUSTOM_DOMAIN%", customDomain);
        request->send(200, "text/html", html.c_str());
    });

    server->on("/pair", HTTP_OPTIONS, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(204);
        response->addHeader("Access-Control-Allow-Origin", "*");
        response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        response->addHeader("Access-Control-Allow-Headers", "Content-Type");
        response->addHeader("Access-Control-Allow-Credentials", "true");
        request->send(response);
    });

    server->on("/pair", HTTP_POST, [this](AsyncWebServerRequest *request) {
    }, nullptr, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) { 
        otaLogger->debug("Pairing request received");
        if (len == 0) {
            AsyncWebServerResponse *response = request->beginResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty request body\"}");
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
            return;
        }

        JsonDocument jsonDoc; 
        DeserializationError error = deserializeJson(jsonDoc, data, len);

        if (error) {
            AsyncWebServerResponse *response = request->beginResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON format\"}");
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
            return;
        }
       
        if (!jsonDoc["user_ids"].is<JsonArray>() || !jsonDoc["wifi_ssid"].is<String>() || 
            !jsonDoc["wifi_password"].is<String>() || !jsonDoc["master_pin"].is<String>()) {
            AsyncWebServerResponse *response = request->beginResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing or invalid keys\"}");
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
            return;
        }

        JsonArray user_ids      = jsonDoc["user_ids"];                                                                                                               // Extract the values from JSON
        String wifi_ssid        = jsonDoc["wifi_ssid"];
        String wifi_password    = jsonDoc["wifi_password"];
        String master_pin       = jsonDoc["master_pin"];

        if (user_ids.size() < 1 || wifi_ssid.isEmpty() || wifi_password.length() < 8 || master_pin.length() < 4) {
            AsyncWebServerResponse *response = request->beginResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Validation failed for one or more fields\"}");
            response->addHeader("Access-Control-Allow-Origin", "*");
            request->send(response);
            return;
        }

        otaLogger->debug("Received Pairing Data:");
        for (size_t i = 0; i < user_ids.size(); i++) {
            otaLogger->debug(String("User ID: " + String((const char*)user_ids[i])).c_str());
        }
        otaLogger->debug(String("WiFi SSID: " + wifi_ssid).c_str());
        otaLogger->debug(String("WiFi Password: " + wifi_password).c_str());
        otaLogger->debug(String("Master PIN: " + master_pin).c_str());

        AsyncWebServerResponse *response;

        if(pairingCallback) {
            pairingCallback(jsonDoc);
            response = request->beginResponse(202, "application/json", "{\"status\":\"success\",\"message\":\"Request Accepted: Listen On Websocket\"}");
        } else {
            otaLogger->warn("Missing Pairing Callback");
            response = request->beginResponse(500, "application/json", "{\"status\":\"error\",\"message\":\"Missing Pairing Functionality\"}");
        }
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
    });
}

bool OTADash::startStation() {
    WiFi.mode(WIFI_STA);
    return connectToWifi(networkCredentials.ssid, networkCredentials.password);
}

bool OTADash::startDualMode() {
    bool result;
    WiFi.mode(WIFI_AP_STA);
    result = WiFi.softAP(String(ssid + String("_AP")).c_str(), password);
    if (!connectToWifi(ssid, password)) {
        otaLogger->error("Failed to connect in Dual mode");
        return false;
    }
    return true && result;
}

bool OTADash::startAccessPoint() {
    WiFi.mode(WIFI_AP);
    return WiFi.softAP(ssid, password);
}

void OTADash::reconnectWifi() {
    disconnectWifi();
    connectToWifi(ssid, password, 5000);
}

void OTADash::handleUpdate(AsyncWebServerRequest *request) {
    bool hasError = Update.hasError();
    int statusCode = hasError ? 500 : 200;                                                // Return 500 on update error
    AsyncWebServerResponse *response = request->beginResponse(
        statusCode,
        "text/plain",
        hasError ? "FAIL" : "OK"
    );
    response->addHeader("Connection", "close");
    request->send(response);

    #if defined(OTA_DASH_PLATFORM_ESP32)
        xTaskCreate([](void *param) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            ESP.restart();
            vTaskDelete(NULL);
        }, "ota_restart", 2048, NULL, 1, NULL);
    #elif defined(OTA_DASH_PLATFORM_ESP8266)
        // Use a simple delay for ESP8266
        delay(2000);
        ESP.restart();
    #endif
}

void OTADash::handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    #if defined(OTA_DASH_PLATFORM_ESP32)
        try {
            if (!index) {
                instance->otaLogger->debug("Update Start: %s\n", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
                    Update.printError(Serial);
                }
            }
            if (!Update.hasError()) {
                if (Update.write(data, len) != len) {
                    // Update.printError(Serial);
                }
            }
            if (final) {
                if (Update.end(true)) {
                    instance->otaLogger->debug("Update Success: %u B\n", index + len);
                } else {
                    // Update.printError(Serial);
                }
            }
        } catch (const std::exception& e) {
            otaLogger->error("Update error: {}", e.what());
        }
    #elif defined(OTA_DASH_PLATFORM_ESP8266)
        if (!index) {
            instance->otaLogger->debug("Update Start: %s\n", filename.c_str());
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace)) {
                Update.printError(Serial);
            }
        }
        if (!Update.hasError()) {
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
        }
        if (final) {
            if (Update.end(true)) {
                instance->otaLogger->debug("Update Success: %u B\n", index + len);
            } else {
                Update.printError(Serial);
            }
        }
    #endif
}

void OTADash::handlePairingResult() {
    String response;
    if(pairResult) {
        otaLogger->debug("Pairing successful");
        response = "{\"status\":\"success\",\"message\":\"Pairing successful\"}";
    } else {
        otaLogger->error("Pairing failed");
        response = "{\"status\":\"error\",\"message\":\"Pairing failed\"}";
    }
    pairRequest = pairResult = false;
    ws->textAll(response);
}

#if defined(OTA_DASH_PLATFORM_ESP32)
void OTADash::otaDashTask(void *parameter) {
    OTADash* dash = static_cast<OTADash*>(parameter);
    unsigned long previousMillis = 0;
    while(dash->serverStarted) {
        dash->handleClient();
        vTaskDelay(10);

        if(millis() - previousMillis >= 10000 && dash->currentMode == NetworkMode::STATION) {
            previousMillis = millis();
            dash->isWifiConnected = WiFi.status() == WL_CONNECTED;

            if (!dash->isWifiConnected) {
                dash->handleNetworkFailure();
            }
        }
    }
    vTaskDelete(NULL);
}
#elif defined(OTA_DASH_PLATFORM_ESP8266)
void OTADash::handleClientTick() {
    if (serverStarted) {
        handleClient();
    }
}
#endif

void OTADash::handleWifiScanResult(int scanResult) {
    if (scanResult == WIFI_SCAN_FAILED) {
        otaLogger->error("Wi-Fi scan failed");
        ws->textAll("[]");
    } else if (scanResult == WIFI_SCAN_RUNNING) {
        otaLogger->debug("Wi-Fi scan still running");
    } else {
        if (scanResult <= 0) {
            otaLogger->warn("Wi-Fi scan returned no results");
            ws->textAll("[]");
            return;
        }

        otaLogger->debug(
            String("Wi-Fi scan completed with " + String(scanResult) + " networks").c_str()
        );
        String networks = "[";
        for (int i = 0; i < scanResult; i++) {
            if (i > 0) networks += ",";
            String ssidEscaped = WiFi.SSID(i);
            ssidEscaped.replace("\"", "\\\""); // Escape quotes
            ssidEscaped.replace("\\", "\\\\"); // Escape backslashes
            networks += "{\"ssid\":\"" + ssidEscaped + "\",";
            networks += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            networks += "\"channel\":" + String(WiFi.channel(i)) + ",";
            networks += "\"encryption\":\"" + encryptionTypeToString(WiFi.encryptionType(i)) + "\"}";
        }
        networks += "]";
        otaLogger->debug("Sending scan results to WebSocket");
        ws->textAll(networks); // Send the results over WebSocket
        WiFi.scanDelete();
    }
    WiFi.scanDelete(); // Clear the scan results
}

void OTADash::handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if(info->final && info->index == 0 && info->len == len) {
        if(info->opcode == WS_TEXT) {
            String message;
            for (size_t i = 0; i < len; i++) {
                message += (char)data[i];
            }
            printDebug("Received message: " + message);
        }
    }
}

void OTADash::handleNetworkFailure() {
    static int reconnectCount = 0;
    static unsigned long lastReconnectAttempt = 0;
    
    if (millis() - lastReconnectAttempt >= reconnectDelay) {
        if (reconnectCount < maxReconnectAttempts) {
            otaLogger->debug("Attempting to reconnect to WiFi...");
            reconnectWifi();
            reconnectCount++;
            lastReconnectAttempt = millis();
        } else {
            otaLogger->error("Max reconnection attempts reached");
            reconnectCount = 0;
        }
    }
}

void OTADash::onWifiSaved(std::function<void(const String&, const String&)> callback) {
    wifiSavedCallback = callback;
}

void OTADash::onPaired(std::function<void(JsonDocument&)> callback) {
    pairingCallback = callback;
}