#!/bin/bash

# Ensure script exits on any error
set -e

# Set base directory paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SERVER_DIR="$PROJECT_ROOT/server"
VENV_DIR="$SERVER_DIR/venv"
REQUIREMENTS_FILE="$SERVER_DIR/requirements.txt"

echo "Script path: $SCRIPT_DIR"
echo "Project root: $PROJECT_ROOT"
echo "Server dir: $SERVER_DIR"
echo "Requirements: $REQUIREMENTS_FILE"

# Check if python3 is installed, otherwise install it
if ! command -v python3 &>/dev/null; then
    echo "python3 not found. Installing Python 3..."
    sudo apt update
    sudo apt install -y python3
fi

# Check if python3-venv is installed
if ! dpkg -s python3-venv &>/dev/null; then
    echo "python3-venv not found. Installing python3-venv..."
    sudo apt install -y python3-venv
fi

# Check if pip is installed, otherwise install it
if ! command -v pip &>/dev/null; then
    echo "pip not found. Installing python3-pip..."
    sudo apt install -y python3-pip
fi

# Update package list
echo "Updating package list..."
sudo apt update

# Install Mosquitto, OpenSSL
echo "Installing Mosquitto, OpenSSL..."
sudo apt install -y mosquitto mosquitto-clients openssl

# Create virtual environment in server/venv
echo "Creating Python virtual environment at: $VENV_DIR"
python3 -m venv "$VENV_DIR"

# Activate the virtual environment
echo "Activating virtual environment..."
source "$VENV_DIR/bin/activate"

# Upgrade pip and install dependencies
echo "Upgrading pip and installing dependencies from: $REQUIREMENTS_FILE"
pip install --upgrade pip
pip install -r "$REQUIREMENTS_FILE"

echo ""
echo "✅ Setup complete."
echo "👉 To activate the environment, run:"
echo "source $VENV_DIR/bin/activate"