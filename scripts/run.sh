#!/bin/bash

# Exit on any error
set -e

# Resolve paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SERVER_DIR="$PROJECT_ROOT/server"
APP_DIR="$SERVER_DIR/app"
VENV_DIR="$SERVER_DIR/venv"
LOG_FORMATTER="$SERVER_DIR/mosquitto_log_format.py"
LOGGING_CONFIG="$SERVER_DIR/logging.yaml"
MOSQUITTO_CONF="$SERVER_DIR/mosquitto.conf"

# Stop any running Python or Mosquitto processes
echo "Stopping existing Python and Mosquitto processes..."
pkill -f "python" || true
pkill -f "mosquitto" || true

# Clear terminal
clear

# Activate virtual environment
echo "Activating virtual environment..."
source "$VENV_DIR/bin/activate"

# Start Mosquitto with custom config and pipe logs to formatter
echo "Starting Mosquitto..."
cd "$SERVER_DIR"
mosquitto -c mosquitto.conf -v 2>&1 | python mosquitto_log_format.py &
MOSQUITTO_PID=$!
echo "Mosquitto started with PID $MOSQUITTO_PID"
cd "$SCRIPT_DIR"

# Define cleanup function to stop Mosquitto on exit
cleanup() {
    echo "Stopping Mosquitto (PID $MOSQUITTO_PID)..."
    kill $MOSQUITTO_PID
}
trap cleanup EXIT

# Start FastAPI app with live-reload
echo "Starting FastAPI (Uvicorn) server..."
cd "$SERVER_DIR"
uvicorn app.main:app \
    --host 127.0.0.1 \
    --port 8000 \
    --reload \
    --log-config logging.yaml \
    --access-log
