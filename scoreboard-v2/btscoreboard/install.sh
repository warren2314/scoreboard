#!/bin/bash
# Install Droylsden CC BT Scoreboard service on the Raspberry Pi
set -e

echo "=== Installing BT Scoreboard ==="

# Install Python dependencies
echo "[1/4] Installing dependencies..."
sudo apt-get install -y python3-dbus python3-gi bluez

# Copy script
echo "[2/4] Installing btscoreboard.py..."
sudo mkdir -p /opt/btscoreboard
sudo cp "$(dirname "$0")/btscoreboard.py" /opt/btscoreboard/btscoreboard.py
sudo chmod 755 /opt/btscoreboard/btscoreboard.py

# Make Bluetooth always discoverable (no timeout)
echo "[3/4] Configuring Bluetooth discoverability..."
sudo sed -i 's/^#*DiscoverableTimeout\s*=.*/DiscoverableTimeout = 0/' /etc/bluetooth/main.conf

# Install and enable systemd service
echo "[4/4] Installing systemd service..."
sudo cp "$(dirname "$0")/btscoreboard.service" /etc/systemd/system/btscoreboard.service

echo ""
echo "IMPORTANT: Edit /etc/systemd/system/btscoreboard.service"
echo "  Change ADMIN_TOKEN=changeme to match your scoreboard admin token"
echo ""

sudo systemctl daemon-reload
sudo systemctl enable btscoreboard
sudo systemctl start btscoreboard

sleep 2
sudo systemctl status btscoreboard --no-pager

echo ""
echo "=== BT Scoreboard installed ==="
echo "Logs: sudo journalctl -u btscoreboard -f"
echo "Device will appear as 'BT-Scoreboard' in the Play Cricket Scorer App"
