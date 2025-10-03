# server/database/models.py

import  uuid
from    typing      import Optional
from    datetime    import datetime
from    sqlmodel    import SQLModel, Field


class Device(SQLModel, table=True):
    device_uuid:                    str = Field(primary_key=True)
    name:                           str = Field(nullable=False, index=True)
    state:                          str = Field(default="off")
    online:                         bool = Field(default=False)  # Online status
    attached_system_uuid:           Optional[str] = None
    created_at: datetime =          Field(default_factory=datetime.utcnow)
    state_updated_at: datetime =    Field(default_factory=datetime.utcnow)


class PendingMessage(SQLModel, table=True):
    id:                             Optional[int] = Field(default=None, primary_key=True)
    device_uuid:                    str = Field(foreign_key="device.device_uuid", index=True)
    message:                        str = Field(nullable=False)  # JSON message content
    created_at: datetime =          Field(default_factory=datetime.utcnow)

# Pydantic Schemas (used in routes)
class DeviceCreate(SQLModel):
    device_uuid: str
    name: str
    attached_system_uuid: Optional[str] = None


class DeviceRead(SQLModel):
    device_uuid: str
    name: str
    state: str
    online: bool  # Include online status in API response
    attached_system_uuid: Optional[str]
    created_at: datetime
    state_updated_at: datetime