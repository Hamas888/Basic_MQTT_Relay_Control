@echo off
setlocal enabledelayedexpansion

REM Set up paths
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%\..
set SERVER_DIR=%PROJECT_ROOT%\server
set VENV_DIR=%SERVER_DIR%\venv
set APP_DIR=%SERVER_DIR%\app
set MOSQUITTO_CONF=%SERVER_DIR%\mosquitto.conf
set LOG_FORMATTER=%SERVER_DIR%\mosquitto_log_format.py
set LOGGING_CONFIG=%SERVER_DIR%\logging.yaml

REM Clear screen
cls

REM Stop any existing Mosquitto and Python processes
echo Stopping existing Mosquitto and Python processes...
taskkill /IM mosquitto.exe /F >nul 2>nul
taskkill /FI "WINDOWTITLE eq Uvicorn*" /F >nul 2>nul

REM Activate virtual environment
echo Activating virtual environment...
call "%VENV_DIR%\Scripts\activate.bat"



REM Start Mosquitto in the background with formatted logging
echo Starting Mosquitto...
cd /d "%SERVER_DIR%"
start /B "" cmd /c ""C:\Program Files\mosquitto\mosquitto.exe" -c mosquitto.conf -v 2>&1 | python mosquitto_log_format.py"
cd /d "%SCRIPT_DIR%"
REM If you want to view Mosquitto logs with color formatting, run:
REM python "%LOG_FORMATTER%" < path_to_mosquitto_log_file


REM Start FastAPI (Uvicorn) server
echo Starting FastAPI server on http://127.0.0.1:8000
cd /d "%SERVER_DIR%"
uvicorn app.main:app --host 127.0.0.1 --port 8000 --reload --log-config "%LOGGING_CONFIG%" --access-log

REM Done
endlocal
