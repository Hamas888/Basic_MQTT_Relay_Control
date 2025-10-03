# server/app/main.py

import  io
import  os
import  asyncio
import  datetime

from    fastapi                     import Request, FastAPI, HTTPException, WebSocket, WebSocketDisconnect, UploadFile, File
from    .routes                     import router
from    .mqtt_client                import mqtt_client_instance

from    utils.logger                import getLogger
from    database.db                 import init_db

app     = FastAPI(title="ESP32 Relay Control Server")
logger  = getLogger("RelaServer")

app.include_router(router)

@app.on_event("startup")
async def startup_event(): 
    init_db()                                                       # Start the database.
    mqtt_client_instance.start()                                    # Start the MQTT client

@app.on_event("shutdown")
async def shutdown_event():
    # # Stop the MQTT client gracefully on shutdown.
    # mqtt_client_instance.stop()
    # # await stream_manager.cleanup() 

    # global udp_transport
    # if udp_transport:
    #     udp_transport.close()
    #     print("[SERVER] Global UDP server closed")
    pass