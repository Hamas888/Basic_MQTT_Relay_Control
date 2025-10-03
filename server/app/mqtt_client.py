# server/app/mqtt_client.py

import  os
import  json
import  asyncio
import  requests
from    typing              import Dict
from    datetime            import datetime
from    paho.mqtt           import client as mqtt_client
from    config              import constants, credentials
from    utils.logger        import getLogger
from    database.db         import get_session
from    database.models     import Device, PendingMessage
from    sqlmodel            import select

logger      = getLogger("MQTTClient")

base_dir    = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ca_cert     = os.path.join(base_dir, credentials.ROOT_CA)
client_cert = os.path.join(base_dir, credentials.CLIENT_CERT)
client_key  = os.path.join(base_dir, credentials.PRIVATE_KEY)

class MQTTClient:
    def __init__(self):
        self.client = mqtt_client.Client()

        self.client.tls_set(
            ca_certs    = ca_cert, 
            certfile    = client_cert, 
            keyfile     = client_key
        )

        self.device_ready: Dict[str, bool] = {}
        self.device_stopped: Dict[str, bool] = {}
        self.client.on_connect  = self.on_connect
        self.client.on_message  = self.on_message
        self.response_callbacks = {}
        try:
            self.loop = asyncio.get_running_loop()
        except RuntimeError:
            self.loop = asyncio.get_event_loop()

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            logger.info("Connected to MQTT Broker!")
            self.client.subscribe(constants.SERVER_SUB_TOPIC)                                                   # Subscribe to uplink messages from devices
            logger.info(f"Subscribed to uplink topic: {constants.SERVER_SUB_TOPIC}")
            self.client.subscribe(constants.SERVER_LWT_TOPIC)    
            logger.info(f"Subscribed to lwt topic: {constants.SERVER_LWT_TOPIC}")                               # Subscribe to uplink messages from devices
        else:
            logger.error("Failed to connect, return code %d", rc)

    def on_message(self, client, userdata, msg):
        # Synchronous handler for Paho MQTT, called from a background thread
        # Use run_coroutine_threadsafe to schedule on the main event loop
        asyncio.run_coroutine_threadsafe(self.handle_message(client, userdata, msg), self.loop)

    async def handle_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode()
        logger.info(f"Received message on topic {topic}: {payload}")

        if topic.startswith(constants.DEVICE_LWT_TOPIC):
            await self.handleStatusMessage(payload)
        
        if topic.startswith(constants.DEVICE_UPLINK_TOPIC):
            await self.handleUplinkMessage(topic, payload)

    async def handleStatusMessage(self, payload):
        try:
            data = json.loads(payload)
            device_id = data.get("device_uuid")
            status = data.get("status")
            if status == "offline" and device_id:
                logger.info(f"Device {device_id} disconnected (offline notification)")
                await self.update_device_online_status(device_id, False)
        except Exception as e:
            logger.error(f"Error handling LWT message: {e}")

    async def update_device_online_status(self, device_uuid: str, online: bool):
        """Update device online status in database"""
        try:
            session = get_session()
            device = session.get(Device, device_uuid)
            if device:
                device.online = online
                session.add(device)
                session.commit()
                session.close()
                logger.info(f"Updated device {device_uuid} online status to {online}")
        except Exception as e:
            logger.error(f"Error updating device online status: {e}")

    async def handleUplinkMessage(self, topic, payload):
        try:
            data = json.loads(payload)
            logger.info(f"message:      {data.get('message', '').lower()}")
            logger.info(f"device_uuid:  {data.get('device_uuid', '').lower()}")
            logger.info(f"state:       {data.get('state', '').lower()}")
        except json.JSONDecodeError: 
            text = payload.lower()
            logger.info(f"message: {text}")
            data = {}

        try:
            _, _, device_uuid = topic.split("/", 2)
        except ValueError:
            logger.error("Unexpected topic format: %s", topic)
            return

        if device_uuid in self.response_callbacks:
            for cb in self.response_callbacks[device_uuid][:]:
                try:
                    cb(data)
                except Exception as e:
                    logger.error(f"Error in response callback: {e}")

        if data.get("message", "").lower() in ("hello", "greetings"):
            logger.info(f"[mqtt_client] Device {device_uuid} connected")
            await self.update_device_online_status(device_uuid, True)
            await self.send_hello(device_uuid)
            await self.send_pending_messages(device_uuid)

    async def send_hello(self, device_uuid: str):
        downlink_topic = constants.SERVER_PUB_TOPIC + device_uuid
        response = {
            "device_id"  : device_uuid,
            "message"    : "hello from server",
        }
        payload = json.dumps(response)
        result = self.client.publish(downlink_topic, payload)
        
        status = result[0]
        if status == 0:
            logger.info(f"Sent hello to device {device_uuid} on topic {downlink_topic}")
        else:
            logger.error(f"Failed to send hello to {downlink_topic}")

    async def send_pending_messages(self, device_uuid: str):
        """Send any pending messages for this device"""
        try:
            session = get_session()
            # Get the latest pending message for this device
            pending = session.exec(
                select(PendingMessage)
                .where(PendingMessage.device_uuid == device_uuid)
                .order_by(PendingMessage.created_at.desc())
            ).first()
            
            if pending:
                logger.info(f"Found pending message for device {device_uuid}")
                downlink_topic = constants.SERVER_PUB_TOPIC + device_uuid
                result = self.client.publish(downlink_topic, pending.message)
                
                if result[0] == 0:
                    logger.info(f"Sent pending message to device {device_uuid}")
                    # Delete all pending messages for this device after successful send
                    pending_messages = session.exec(
                        select(PendingMessage).where(PendingMessage.device_uuid == device_uuid)
                    ).all()
                    for msg in pending_messages:
                        session.delete(msg)
                    session.commit()
                else:
                    logger.error(f"Failed to send pending message to device {device_uuid}")
            
            session.close()
        except Exception as e:
            logger.error(f"Error sending pending messages: {e}")

    async def send_state_command(self, device_uuid: str, state: str) -> bool:
        """Send state change command and wait for acknowledgment"""
        downlink_topic = constants.SERVER_PUB_TOPIC + device_uuid
        command = {
            "device_id": device_uuid,
            "command": "set_state",
            "state": state,
            "timestamp": datetime.utcnow().isoformat()
        }
        payload = json.dumps(command)
        
        # Set up callback for acknowledgment
        ack_received = asyncio.Event()
        
        def ack_callback(response_data):
            if response_data.get("command") == "ack" and response_data.get("state") == state:
                ack_received.set()
        
        # Register callback
        if device_uuid not in self.response_callbacks:
            self.response_callbacks[device_uuid] = []
        self.response_callbacks[device_uuid].append(ack_callback)
        
        try:
            # Send command
            result = self.client.publish(downlink_topic, payload)
            if result[0] != 0:
                logger.error(f"Failed to send state command to device {device_uuid}")
                return False
            
            logger.info(f"Sent state command to device {device_uuid}: {state}")
            
            # Wait for acknowledgment (5 second timeout)
            try:
                await asyncio.wait_for(ack_received.wait(), timeout=5.0)
                logger.info(f"Received acknowledgment from device {device_uuid}")
                return True
            except asyncio.TimeoutError:
                logger.warning(f"No acknowledgment from device {device_uuid} within 5 seconds")
                # Store as pending message
                await self.store_pending_message(device_uuid, payload)
                return False
                
        finally:
            # Clean up callback
            if device_uuid in self.response_callbacks:
                try:
                    self.response_callbacks[device_uuid].remove(ack_callback)
                except ValueError:
                    pass

    async def store_pending_message(self, device_uuid: str, message: str):
        """Store message for later delivery when device comes online"""
        try:
            session = get_session()
            
            # Delete any existing pending messages for this device (keep only latest)
            existing = session.exec(
                select(PendingMessage).where(PendingMessage.device_uuid == device_uuid)
            ).all()
            for msg in existing:
                session.delete(msg)
            
            # Add new pending message
            pending = PendingMessage(
                device_uuid=device_uuid,
                message=message
            )
            session.add(pending)
            session.commit()
            session.close()
            logger.info(f"Stored pending message for device {device_uuid}")
        except Exception as e:
            logger.error(f"Error storing pending message: {e}")

    def start(self):
        logger.info(f"Connecting to MQTT broker at {constants.MQTT_BROKER}:{constants.SERVER_PORT}")
        self.client.connect(constants.MQTT_BROKER, constants.SERVER_PORT, constants.MQTT_KEEPALIVE)
        # Run the MQTT network loop in the background.
        self.client.loop_start()
        logger.info("MQTT client started successfully")

    def stop(self):
        self.client.loop_stop()
        self.client.disconnect()

    def send_command(self, device_uuid: str, command_payload: str):
        """
        Send a command to a device by publishing on its unique downlink topic.
        """
        downlink_topic = constants.SERVER_PUB_TOPIC + device_uuid
        result = self.client.publish(downlink_topic, command_payload)
        status = result[0]
        if status == 0:
            logger.info(f"Sent command to device {device_uuid} on topic {downlink_topic}: {command_payload}")
        else:
            logger.error(f"Failed to send command to topic {downlink_topic}")
    
    def register_response_callback(self, device_uuid, callback):
        if device_uuid not in self.response_callbacks:
            self.response_callbacks[device_uuid] = []
        self.response_callbacks[device_uuid].append(callback)

    def unregister_response_callback(self, device_uuid, callback):
        if device_uuid in self.response_callbacks:
            try:
                self.response_callbacks[device_uuid].remove(callback)
                if not self.response_callbacks[device_uuid]:
                    del self.response_callbacks[device_uuid]
            except ValueError:
                pass

mqtt_client_instance = MQTTClient()                                                 # Create a single instance that can be imported elsewhere