@echo off
setlocal enabledelayedexpansion

REM Set up paths
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%\..
set SERVER_DIR=%PROJECT_ROOT%\server
set VENV_DIR=%SERVER_DIR%\venv
set REQUIREMENTS_FILE=%SERVER_DIR%\requirements.txt

echo Script dir: %SCRIPT_DIR%
echo Project root: %PROJECT_ROOT%
echo Server dir: %SERVER_DIR%
echo Requirements file: %REQUIREMENTS_FILE%

REM Check if python is available
where python >nul 2>nul
if errorlevel 1 (
    echo Python not found. Please install Python 3 and add it to PATH.
    exit /b 1
)

REM Create virtual environment
echo Creating virtual environment in %VENV_DIR%
python -m venv "%VENV_DIR%"

REM Activate virtual environment
call "%VENV_DIR%\Scripts\activate.bat"

REM Upgrade pip and install requirements
echo Upgrading pip...
python -m pip install --upgrade pip

if exist "%REQUIREMENTS_FILE%" (
    echo Installing requirements...
    pip install -r "%REQUIREMENTS_FILE%"
) else (
    echo Requirements file not found: %REQUIREMENTS_FILE%
)

echo.
echo Setup complete.
echo To activate the environment later, run:
echo call "%VENV_DIR%\Scripts\activate.bat"

echo.
echo ------------------------------------------------------------
echo NOTE FOR WINDOWS USERS:
echo - This script is designed for Command Prompt (CMD), not PowerShell.
echo - Packages like mosquitto and openssl are NOT installed automatically.
echo - You must install those separately using:
echo     - Chocolatey (https://chocolatey.org/)
echo     - Scoop (https://scoop.sh/)
echo     - Or manual download from their official websites.
echo ------------------------------------------------------------