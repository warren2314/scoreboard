#!/bin/bash
FQBN="arduino:renesas_uno:unor4wifi"
SKETCH="/home/warren/scoreboard/scoreboard-v2/arduino/scoreboard/scoreboard.ino"

echo "Waiting for bootloader... double-tap the reset button on the Arduino"

while true; do
  for port in /dev/ttyACM*; do
    if [ -e "$port" ]; then
      echo "Found port $port, attempting upload..."
      arduino-cli upload -p "$port" --fqbn "$FQBN" "$SKETCH" && echo "Upload successful!" && exit 0
    fi
  done
  sleep 0.5
done
