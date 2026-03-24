#!/bin/bash
#
# Deploy modernized cricket scoreboard to Raspberry Pi 3 B+
# Deploys BOTH the Node.js server AND uploads the Arduino sketch
#
# Usage:
#   ./deploy.sh <pi-ip-address> [pi-username]
#
# Examples:
#   ./deploy.sh 192.168.1.50
#   ./deploy.sh 192.168.1.50 pi
#
# Prerequisites:
#   - Pi is on the network with SSH enabled
#   - Arduino is plugged into the Pi via USB (USB A-to-B cable)

set -e

PI_HOST="${1:?Usage: $0 <pi-ip-address> [pi-username]}"
PI_USER="${2:-pi}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Cricket Scoreboard v2 Deployment ==="
echo "Target: ${PI_USER}@${PI_HOST}"
echo ""

# Step 1: Copy ALL files to Pi (server + arduino)
echo "[1/4] Copying files to Pi..."
ssh "${PI_USER}@${PI_HOST}" "mkdir -p /tmp/scoreboard-upload/arduino && sudo mkdir -p /opt/scoreboard"
scp -r "${SCRIPT_DIR}/server/"* "${PI_USER}@${PI_HOST}:/tmp/scoreboard-upload/"
scp -r "${SCRIPT_DIR}/arduino/"* "${PI_USER}@${PI_HOST}:/tmp/scoreboard-upload/arduino/"
scp "${SCRIPT_DIR}/scoreboard.service" "${PI_USER}@${PI_HOST}:/tmp/scoreboard.service"

# Step 2: Install Node.js server
echo "[2/4] Installing Node.js server..."
ssh "${PI_USER}@${PI_HOST}" bash -s <<'REMOTE_SCRIPT'
set -e

echo "--- Moving server files into place ---"
sudo mkdir -p /opt/scoreboard
sudo cp -r /tmp/scoreboard-upload/package.json /opt/scoreboard/
sudo cp -r /tmp/scoreboard-upload/server.js /opt/scoreboard/
sudo cp -r /tmp/scoreboard-upload/public /opt/scoreboard/
sudo cp /tmp/scoreboard.service /etc/systemd/system/scoreboard.service

echo "--- Installing Node.js (if not present) ---"
if ! command -v node &>/dev/null; then
    echo "Node.js not found. Installing via NodeSource..."
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt-get install -y nodejs
else
    echo "Node.js already installed: $(node --version)"
fi

echo "--- Installing build tools (required for serialport native bindings) ---"
sudo apt-get install -y build-essential python3

echo "--- Installing npm dependencies ---"
cd /opt/scoreboard
sudo npm install --production

echo "--- Setting permissions ---"
sudo chown -R root:root /opt/scoreboard

echo "--- Configuring sudoers for shutdown/reboot ---"
sudo bash -c 'cat > /etc/sudoers.d/scoreboard <<EOF
root ALL=(ALL) NOPASSWD: /sbin/shutdown
EOF'
sudo chmod 440 /etc/sudoers.d/scoreboard

echo "--- Stopping old services if present ---"
sudo systemctl stop apache2 2>/dev/null || true
sudo systemctl disable apache2 2>/dev/null || true
(sudo crontab -l 2>/dev/null | grep -v scoreboard) | sudo crontab - 2>/dev/null || true

echo "--- Enabling and starting scoreboard service ---"
sudo systemctl daemon-reload
sudo systemctl enable scoreboard
sudo systemctl restart scoreboard

sleep 2
sudo systemctl status scoreboard --no-pager
echo ""
echo "--- Node.js server installed ---"
REMOTE_SCRIPT

# Step 3: Upload Arduino sketch from the Pi
echo ""
echo "[3/4] Uploading Arduino sketch from Pi..."
ssh "${PI_USER}@${PI_HOST}" bash -s <<'ARDUINO_SCRIPT'
set -e

echo "--- Installing arduino-cli (if not present) ---"
if ! command -v arduino-cli &>/dev/null; then
    echo "Installing arduino-cli..."
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sudo BINDIR=/usr/local/bin sh
else
    echo "arduino-cli already installed: $(arduino-cli version)"
fi

echo "--- Setting up Arduino core and library ---"
arduino-cli config init --overwrite 2>/dev/null || true
arduino-cli core update-index
arduino-cli core install arduino:renesas_uno 2>/dev/null || echo "arduino:renesas_uno core already installed"

# Install the ShifterStr library
echo "--- Installing ShifterStr library ---"
ARDUINO_LIB_DIR="$(arduino-cli config dump --format json | grep -o '"user": "[^"]*"' | head -1 | cut -d'"' -f4)/libraries"
if [ -z "$ARDUINO_LIB_DIR" ] || [ "$ARDUINO_LIB_DIR" = "/libraries" ]; then
    ARDUINO_LIB_DIR="$HOME/Arduino/libraries"
fi
mkdir -p "$ARDUINO_LIB_DIR/ShifterStr"
cp /tmp/scoreboard-upload/arduino/lib/ShifterStr/* "$ARDUINO_LIB_DIR/ShifterStr/"
echo "ShifterStr installed to $ARDUINO_LIB_DIR/ShifterStr/"

# Copy sketch to a temp build directory
mkdir -p /tmp/scoreboard-sketch/scoreboard
cp /tmp/scoreboard-upload/arduino/scoreboard/scoreboard.ino /tmp/scoreboard-sketch/scoreboard/

echo "--- Detecting Arduino board ---"
BOARD_PORT=""
for attempt in 1 2 3; do
    BOARD_INFO=$(arduino-cli board list 2>/dev/null | grep -i "ttyACM\|ttyUSB" | head -1)
    if [ -n "$BOARD_INFO" ]; then
        BOARD_PORT=$(echo "$BOARD_INFO" | awk '{print $1}')
        break
    fi
    echo "No board detected, retrying in 2s... (attempt $attempt/3)"
    sleep 2
done

if [ -z "$BOARD_PORT" ]; then
    echo ""
    echo "ERROR: No Arduino detected on USB."
    echo "Make sure the Arduino is plugged into the Pi with a USB cable."
    echo ""
    echo "You can upload manually later with:"
    echo "  arduino-cli compile -b arduino:renesas_uno:unor4wifi /tmp/scoreboard-sketch/scoreboard"
    echo "  arduino-cli upload -b arduino:renesas_uno:unor4wifi -p /dev/ttyACM0 /tmp/scoreboard-sketch/scoreboard"
    exit 1
fi

echo "Found Arduino on $BOARD_PORT"

echo "--- Compiling sketch ---"
arduino-cli compile -b arduino:renesas_uno:unor4wifi /tmp/scoreboard-sketch/scoreboard

echo "--- Stopping scoreboard service to free serial port ---"
sudo systemctl stop scoreboard 2>/dev/null || true
sleep 2

echo "--- Uploading to Arduino on $BOARD_PORT ---"
arduino-cli upload -b arduino:renesas_uno:unor4wifi -p "$BOARD_PORT" /tmp/scoreboard-sketch/scoreboard

echo "--- Restarting scoreboard service ---"
sudo systemctl start scoreboard

echo ""
echo "--- Arduino sketch uploaded successfully! ---"

# Cleanup
rm -rf /tmp/scoreboard-upload /tmp/scoreboard-sketch /tmp/scoreboard.service
ARDUINO_SCRIPT

# Step 4: Verify
echo ""
echo "[4/4] Verifying..."
sleep 2
if curl -s --connect-timeout 5 "http://${PI_HOST}/" | grep -qi "scoreboard"; then
    echo "SUCCESS: Scoreboard is live at http://${PI_HOST}/"
else
    echo "WARNING: Could not verify. Check http://${PI_HOST}/ manually."
    echo "Debug: ssh ${PI_USER}@${PI_HOST} sudo journalctl -u scoreboard -f"
fi

echo ""
echo "========================================"
echo "  Scoreboard:  http://${PI_HOST}/"
echo "  Admin panel: http://${PI_HOST}/admin"
echo ""
echo "  IMPORTANT: Change the admin token!"
echo "  Edit /etc/systemd/system/scoreboard.service"
echo "  Change ADMIN_TOKEN=changeme to something secure"
echo "  Then: sudo systemctl daemon-reload && sudo systemctl restart scoreboard"
echo ""
echo "  Logs: ssh ${PI_USER}@${PI_HOST} sudo journalctl -u scoreboard -f"
echo "========================================"
