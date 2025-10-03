#!/usr/bin/env python3
"""
ESP32 Device Simulator for MQTT Relay Control Testing
Simulates an ESP32 device with relay control capabilities
"""

import json
import time
import random
import ssl
import paho.mqtt.client as mqtt
from datetime import datetime

# Configuration
DEVICE_UUID = "AABBCCDDEEF1"
DEVICE_NAME = "Test ESP32 Relay"
MQTT_BROKER = "localhost"
MQTT_PORT = 8883
MQTT_KEEPALIVE = 60

# Topics
UPLINK_TOPIC = f"ControlDevice/Uplink/{DEVICE_UUID}"
DOWNLINK_TOPIC = f"ControlDevice/Downlink/{DEVICE_UUID}"
STATUS_TOPIC = f"ControlDevice/Status/{DEVICE_UUID}"

# Certificate paths (use your existing certs)
CA_CERT = "server/certs/ca.crt"
CLIENT_CERT = "server/certs/client.crt"
CLIENT_KEY = "server/certs/client.key"

class ESP32Simulator:
    def __init__(self):
        self.device_uuid = DEVICE_UUID
        self.device_name = DEVICE_NAME
        self.relay_state = "off"
        # Use the newer callback API version to avoid deprecation warning
        self.client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
        self.setup_mqtt()
        
    def setup_mqtt(self):
        """Setup MQTT client with TLS"""
        # Setup TLS
        self.client.tls_set(
            ca_certs=CA_CERT,
            certfile=CLIENT_CERT,
            keyfile=CLIENT_KEY,
            cert_reqs=ssl.CERT_REQUIRED,
            tls_version=ssl.PROTOCOL_TLS,
            ciphers=None
        )
        
        # Setup callbacks
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        # Setup Last Will and Testament
        lwt_message = {
            "device_uuid": self.device_uuid,
            "status": "offline",
            "timestamp": datetime.utcnow().isoformat()
        }
        self.client.will_set(STATUS_TOPIC, json.dumps(lwt_message), qos=1, retain=True)
        
    def on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            print(f"❌ Failed to connect: {reason_code}")
        else:
            print(f"✅ Connected to MQTT broker (Code: {reason_code})")
            
            # Subscribe to downlink topic
            client.subscribe(DOWNLINK_TOPIC)
            print(f"📡 Subscribed to: {DOWNLINK_TOPIC}")
            
            # Send hello message
            self.send_hello()
            
            # Send online status
            self.send_status("online")
            
    def on_message(self, client, userdata, msg):
        try:
            payload = msg.payload.decode()
            data = json.loads(payload)
            print(f"📥 Received: {payload}")
            
            command = data.get("command")
            
            if command == "set_state":
                new_state = data.get("state")
                if new_state in ["on", "off"]:
                    print(f"🔄 Changing relay state from {self.relay_state} to {new_state}")
                    self.relay_state = new_state
                    
                    # Simulate relay switching delay
                    time.sleep(0.5)
                    
                    # Send acknowledgment
                    self.send_ack(new_state)
                    print(f"✅ Relay state changed to: {self.relay_state}")
                else:
                    print(f"❌ Invalid state: {new_state}")
                    
            elif data.get("message") == "hello from server":
                print("👋 Received hello from server")
                
        except Exception as e:
            print(f"❌ Error processing message: {e}")
            
    def on_disconnect(self, client, userdata, reason_code, properties=None):
        if reason_code.is_failure:
            print(f"❌ Unexpected disconnection: {reason_code}")
        else:
            print(f"📤 Disconnected from MQTT broker (Code: {reason_code})")
        
    def send_hello(self):
        """Send hello message to announce device presence"""
        message = {
            "device_uuid": self.device_uuid,
            "message": "hello",
            "device_name": self.device_name,
            "state": self.relay_state,
            "timestamp": datetime.utcnow().isoformat()
        }
        self.client.publish(UPLINK_TOPIC, json.dumps(message))
        print(f"👋 Sent hello message")
        
    def send_ack(self, state):
        """Send acknowledgment for state change"""
        message = {
            "device_uuid": self.device_uuid,
            "command": "ack",
            "state": state,
            "timestamp": datetime.utcnow().isoformat()
        }
        self.client.publish(UPLINK_TOPIC, json.dumps(message))
        print(f"✅ Sent ACK for state: {state}")
        
    def send_status(self, status):
        """Send device status (online/offline)"""
        message = {
            "device_uuid": self.device_uuid,
            "status": status,
            "timestamp": datetime.utcnow().isoformat()
        }
        self.client.publish(STATUS_TOPIC, json.dumps(message), retain=True)
        print(f"📊 Sent status: {status}")
        
    def send_heartbeat(self):
        """Send periodic heartbeat"""
        message = {
            "device_uuid": self.device_uuid,
            "message": "heartbeat",
            "state": self.relay_state,
            "uptime": time.time(),
            "timestamp": datetime.utcnow().isoformat()
        }
        self.client.publish(UPLINK_TOPIC, json.dumps(message))
        print(f"💓 Sent heartbeat - State: {self.relay_state}")
        
    def run(self):
        """Main run loop"""
        try:
            print(f"🚀 Starting ESP32 Simulator")
            print(f"📱 Device UUID: {self.device_uuid}")
            print(f"🏷️  Device Name: {self.device_name}")
            print(f"🌐 Broker: {MQTT_BROKER}:{MQTT_PORT}")
            print(f"📡 Uplink Topic: {UPLINK_TOPIC}")
            print(f"📡 Downlink Topic: {DOWNLINK_TOPIC}")
            print("-" * 50)
            
            # Connect to broker
            self.client.connect(MQTT_BROKER, MQTT_PORT, MQTT_KEEPALIVE)
            
            # Start network loop
            self.client.loop_start()
            
            # Main loop with periodic heartbeat
            last_heartbeat = time.time()
            heartbeat_interval = 30  # 30 seconds
            
            while True:
                current_time = time.time()
                
                # Send heartbeat every 30 seconds
                if current_time - last_heartbeat >= heartbeat_interval:
                    self.send_heartbeat()
                    last_heartbeat = current_time
                
                time.sleep(1)
                
        except KeyboardInterrupt:
            print("\\n🛑 Shutting down...")
            self.send_status("offline")
            self.client.loop_stop()
            self.client.disconnect()
            print("✅ Disconnected cleanly")

if __name__ == "__main__":
    simulator = ESP32Simulator()
    simulator.run()