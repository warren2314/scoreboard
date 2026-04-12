#!/bin/bash
FQBN="arduino:renesas_uno:unor4wifi"
SKETCH="/home/warren/scoreboard/scoreboard-v2/arduino/scoreboard/scoreboard.ino"

echo "Waiting for double-tap reset... (watch for port to disappear then reappear)"

# Wait for port to disappear (the reset)
echo "Step 1: Double-tap the reset button now..."
while [ -e /dev/ttyACM0 ]; do sleep 0.2; done
echo "Port disappeared - board is resetting..."

# Wait for port to reappear (bootloader mode)
echo "Step 2: Waiting for bootloader port..."
while [ ! -e /dev/ttyACM0 ]; do sleep 0.2; done
echo "Bootloader detected! Uploading..."

arduino-cli upload -p /dev/ttyACM0 --fqbn "$FQBN" "$SKETCH" && echo "Upload successful!" || echo "Upload failed - try again"
