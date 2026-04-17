#!/bin/bash
#
# Deploy Droylsden CC cricket scoreboard — run this directly on the Pi
# Deploys: Node.js server, BT Scoreboard service, and Arduino sketch
#
# Usage (on the Pi):
#   git pull
#   cd scoreboard-v2
#   chmod +x deploy.sh
#   ./deploy.sh
#
# Prerequisites:
#   - Arduino is plugged into the Pi via USB

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Cricket Scoreboard v2 Deployment ==="
echo ""

# Step 1: Install Node.js server
echo "[1/4] Installing Node.js server..."

echo "--- Moving server files into place ---"
sudo mkdir -p /opt/scoreboard
sudo cp -r "${SCRIPT_DIR}/server/package.json" /opt/scoreboard/
sudo cp -r "${SCRIPT_DIR}/server/server.js" /opt/scoreboard/
sudo cp -r "${SCRIPT_DIR}/server/public" /opt/scoreboard/
sudo cp "${SCRIPT_DIR}/scoreboard.service" /etc/systemd/system/scoreboard.service

echo "--- Installing Node.js (if not present) ---"
if ! command -v node &>/dev/null; then
    echo "Node.js not found. Installing via NodeSource..."
    curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
    sudo apt-get install -y nodejs
else
    echo "Node.js already installed: $(node --version)"
fi

echo "--- Installing build tools ---"
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

echo "--- Enabling and starting scoreboard service ---"
sudo systemctl daemon-reload
sudo systemctl enable scoreboard
sudo systemctl restart scoreboard

sleep 2
sudo systemctl status scoreboard --no-pager
echo ""
echo "--- Node.js server installed ---"

# Step 2: Install BT Scoreboard service
echo ""
echo "[2/4] Installing BT Scoreboard service..."

echo "--- Installing Python BLE dependencies ---"
sudo apt-get install -y python3-dbus python3-gi bluez

echo "--- Installing btscoreboard.py ---"
sudo mkdir -p /opt/btscoreboard
sudo cp "${SCRIPT_DIR}/btscoreboard/btscoreboard.py" /opt/btscoreboard/btscoreboard.py
sudo chmod 755 /opt/btscoreboard/btscoreboard.py

echo "--- Configuring Bluetooth (always discoverable) ---"
sudo sed -i 's/^#*DiscoverableTimeout\s*=.*/DiscoverableTimeout = 0/' /etc/bluetooth/main.conf

echo "--- Installing btscoreboard systemd service ---"
sudo cp "${SCRIPT_DIR}/btscoreboard/btscoreboard.service" /etc/systemd/system/btscoreboard.service

echo "--- Enabling btscoreboard service ---"
sudo systemctl daemon-reload
sudo systemctl enable btscoreboard
sudo systemctl restart btscoreboard

sleep 2
sudo systemctl status btscoreboard --no-pager
echo ""
echo "--- BT Scoreboard installed ---"

# Step 3: Flash Arduino sketch
echo ""
echo "[3/4] Flashing Arduino sketch..."

echo "--- Installing arduino-cli (if not present) ---"
if ! command -v arduino-cli &>/dev/null; then
    echo "Installing arduino-cli..."
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sudo BINDIR=/usr/local/bin sh
else
    echo "arduino-cli already installed: $(arduino-cli version)"
fi

echo "--- Setting up Arduino UNO R4 core ---"
arduino-cli config init --overwrite 2>/dev/null || true
arduino-cli core update-index
arduino-cli core install arduino:renesas_uno 2>/dev/null || echo "Core already installed"

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
    echo "WARNING: No Arduino detected on USB — skipping flash."
    echo "To flash manually later:"
    echo "  arduino-cli compile -b arduino:renesas_uno:unor4wifi ${SCRIPT_DIR}/arduino/scoreboard"
    echo "  arduino-cli upload -b arduino:renesas_uno:unor4wifi -p /dev/ttyACM0 ${SCRIPT_DIR}/arduino/scoreboard"
else
    echo "Found Arduino on $BOARD_PORT"

    echo "--- Compiling sketch ---"
    arduino-cli compile -b arduino:renesas_uno:unor4wifi "${SCRIPT_DIR}/arduino/scoreboard"

    echo "--- Stopping scoreboard service to free serial port ---"
    sudo systemctl stop scoreboard 2>/dev/null || true
    sleep 2

    echo "--- Uploading to Arduino ---"
    arduino-cli upload -b arduino:renesas_uno:unor4wifi -p "$BOARD_PORT" "${SCRIPT_DIR}/arduino/scoreboard"

    echo "--- Restarting scoreboard service ---"
    sudo systemctl start scoreboard

    echo "--- Arduino sketch uploaded successfully ---"
fi

# Step 4: Verify
echo ""
echo "[4/4] Verifying..."
sleep 2
if curl -s --connect-timeout 5 "http://localhost/" | grep -qi "scoreboard"; then
    echo "SUCCESS: Scoreboard is live"
else
    echo "WARNING: Could not verify. Check: sudo journalctl -u scoreboard -f"
fi

echo ""
echo "========================================"
echo "  Scoreboard:  http://$(hostname -I | awk '{print $1}')/"
echo "  Admin panel: http://$(hostname -I | awk '{print $1}')/admin"
echo ""
echo "  IMPORTANT: Set the same admin token in BOTH service files:"
echo "  sudo nano /etc/systemd/system/scoreboard.service"
echo "  sudo nano /etc/systemd/system/btscoreboard.service"
echo "  Change ADMIN_TOKEN=changeme to something secure in both"
echo "  Then: sudo systemctl daemon-reload"
echo "        sudo systemctl restart scoreboard btscoreboard"
echo ""
echo "  BT device name: BT-Scoreboard"
echo "  In Play Cricket Scorer App: External Scoreboard → Generic → Not Connected"
echo ""
echo "  Logs: sudo journalctl -u scoreboard -f"
echo "        sudo journalctl -u btscoreboard -f"
echo "========================================"
