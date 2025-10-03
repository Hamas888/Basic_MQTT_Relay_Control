# server/app/routes.py

import  json
import  asyncio
import  requests

from    enum               import Enum
from    uuid               import uuid4
from    sqlmodel           import select
from    datetime           import datetime
from    database.db        import get_session
from    fastapi            import APIRouter, Request, HTTPException, status
from    utils.logger       import getLogger
from    database.models    import Device, DeviceCreate, DeviceRead
from    fastapi.responses  import JSONResponse
from    .mqtt_client       import mqtt_client_instance

logger          = getLogger("Routes")
router          = APIRouter()


class DeviceState(str, Enum):
    on = "on"
    off = "off"

# API Endpoints

# Create Device
@router.post("/ControlDevice/create_device", response_model=DeviceRead)
async def create_device(payload: DeviceCreate):
    logger.info(f"create device -> payload: {payload.json()}")

    session = get_session()

    # Check if the UUID is already in use
    existing = session.get(Device, payload.device_uuid)
    if existing:
        raise HTTPException(status_code=400, detail="Device with this UUID already exists")

    # Optional: check duplicate name
    existing_name = session.exec(select(Device).where(Device.name == payload.name)).first()
    if existing_name:
        raise HTTPException(status_code=400, detail="Device with this name already exists")

    device = Device(
        device_uuid=payload.device_uuid,
        name=payload.name,
        attached_system_uuid=payload.attached_system_uuid
    )

    session.add(device)
    session.commit()
    session.refresh(device)

    return device

# Delete Device
@router.delete("/ControlDevice/delete_device/{device_uuid}")
async def delete_device(device_uuid: str):
    session = get_session()

    device = session.get(Device, device_uuid)
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    session.delete(device)
    session.commit()

    logger.info(f"Deleted device id {device_uuid}")
    return {"device_uuid": device_uuid, "status": "deleted"}

# Fetch Device
@router.get("/ControlDevice/fetch_device_info/{device_uuid}", response_model=DeviceRead)
async def get_device_info(device_uuid: str):
    session = get_session()
    device = session.get(Device, device_uuid)

    if not device:
        raise HTTPException(status_code=404, detail="Device not found")

    logger.info(f"Fetched device id {device_uuid} info")
    return device

# Set Device State
@router.get("/ControlDevice/set_state/{device_uuid}")
async def set_device_state(device_uuid: str, state: DeviceState):
    session = get_session()
    device = session.get(Device, device_uuid)

    if not device:
        session.close()
        raise HTTPException(status_code=404, detail="Device not found")

    logger.info(f"Setting device {device_uuid} state to {state.value}")
    
    # Send MQTT command and wait for acknowledgment
    ack_received = await mqtt_client_instance.send_state_command(device_uuid, state.value)
    
    if ack_received:
        # Update database only if device acknowledged
        device.state = state.value
        device.state_updated_at = datetime.utcnow()
        session.add(device)
        session.commit()
        session.refresh(device)
        session.close()
        
        logger.info(f"Device {device_uuid} acknowledged state change to {state.value}")
        return {
            "device_uuid": device.device_uuid,
            "state": device.state,
            "acknowledged": True,
            "state_updated_at": device.state_updated_at.isoformat() + "Z"
        }
    else:
        session.close()
        logger.warning(f"Device {device_uuid} did not acknowledge state change - message stored as pending")
        return {
            "device_uuid": device_uuid,
            "state": state.value,
            "acknowledged": False,
            "message": "Device did not respond - command will be sent when device comes online"
        }