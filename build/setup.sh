#!/bin/bash

# Setup script for LOADSTAR & LUNA-ORBIT
# Creates virtual environment and installs dependencies

set -e  # Exit on error

echo "Starting setup..."

# Check Python version
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 could not be found."
    exit 1
fi

# Create virtual environment
if [ ! -d "venv" ]; then
    echo "Creating virtual environment 'venv'..."
    python3 -m venv venv
else
    echo "Virtual environment 'venv' already exists."
fi

# Activate virtual environment
source venv/bin/activate

# Install dependencies
if [ -f "requirements.txt" ]; then
    echo "Installing dependencies from requirements.txt..."
    pip install -r requirements.txt
else
    echo "Warning: requirements.txt not found!"
fi

#Install libboost-dev
sudo apt-get install libboost-dev

# Create output directories
echo "Creating output directories..."
mkdir -p outputs
mkdir -p src/loadstar/outputs
mkdir -p src/luna_orbit/outputs

echo "Setup complete! Activate the environment with: source venv/bin/activate"
