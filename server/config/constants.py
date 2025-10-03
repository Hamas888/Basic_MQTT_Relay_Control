# Device Configuration
SERVER_URL                          = "localhost"
SERVER_PORT                         = 8883
MQTT_BROKER                         = "localhost"
MQTT_KEEPALIVE                      = 60
SERVER_VERSION                      = "1.0.0"
SERVER_PUB_TOPIC                    = "ControlDevice/Downlink/"
SERVER_SUB_TOPIC                    = "ControlDevice/Uplink/+"
SERVER_LWT_TOPIC                    = "ControlDevice/Status/+"
DEVICE_LWT_TOPIC                    = "ControlDevice/Status/"
DEVICE_UPLINK_TOPIC                 = "ControlDevice/Uplink/"
DEVICE_DOWNLINK_TOPIC               = "ControlDevice/Downlink/"

# API Endpoints (WebApp)

SET_CONTROL_DEVICE_API_ENDPOINT     = "/ControlDevice/set_state/"
FETCH_CONTROL_DEVICE_API_ENDPOINT   = "/ControlDevice/fetch_device_info/"
CREATE_CONTROL_DEVICE_API_ENDPOINT  = "/ControlDevice/create_device"
DELETE_CONTROL_DEVICE_API_ENDPOINT  = "/ControlDevice/delete_device/"