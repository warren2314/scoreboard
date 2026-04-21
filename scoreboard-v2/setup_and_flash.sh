#!/bin/bash
set -e

FQBN="arduino:renesas_uno:unor4wifi"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SKETCH="$SCRIPT_DIR/arduino/scoreboard/scoreboard.ino"
CLI_DIR="$HOME/.local/bin"
CLI="$CLI_DIR/arduino-cli"

echo "=== Droylsden CC Scoreboard Setup Tool ==="
echo ""

# ── Step 0: Pull latest files from warren branch ─────────────────────────────
echo "[0/6] Fetching latest files from warren branch..."
git -C "$SCRIPT_DIR/.." fetch origin warren 2>/dev/null || true
git -C "$SCRIPT_DIR/.." show origin/warren:scoreboard-v2/arduino/scoreboard/scoreboard.ino > "$SKETCH"
git -C "$SCRIPT_DIR/.." show origin/warren:scoreboard-v2/btscoreboard/btscoreboard.py > /tmp/btscoreboard.py
git -C "$SCRIPT_DIR/.." show origin/warren:scoreboard-v2/btscoreboard/btscoreboard.service > /tmp/btscoreboard.service
echo "      Files updated from warren branch"
echo ""

# ── Step 1: Install arduino-cli if missing ───────────────────────────────────
if ! command -v arduino-cli &>/dev/null && [ ! -x "$CLI" ]; then
  echo "[1/6] Installing arduino-cli..."
  mkdir -p "$CLI_DIR"
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$CLI_DIR" sh
  export PATH="$CLI_DIR:$PATH"
  echo "      arduino-cli installed to $CLI_DIR"
else
  export PATH="$CLI_DIR:$PATH"
  echo "[1/6] arduino-cli already installed ($(arduino-cli version | head -1))"
fi

# ── Step 2: Install R4 core if missing ───────────────────────────────────────
echo "[2/6] Checking Arduino R4 core..."
if ! arduino-cli core list | grep -q "arduino:renesas_uno"; then
  echo "      Installing arduino:renesas_uno core (this may take a few minutes)..."
  arduino-cli core update-index
  arduino-cli core install arduino:renesas_uno
else
  echo "      Core already installed"
fi

# ── Step 3: Compile ───────────────────────────────────────────────────────────
echo "[3/6] Compiling sketch..."
arduino-cli compile --fqbn "$FQBN" "$SKETCH"
echo "      Compile successful"

# ── Step 4: Upload (wait for bootloader) ─────────────────────────────────────
echo ""
echo "[4/6] Ready to upload"
echo "      >>> Double-tap the RESET button on the Arduino now <<<"
echo "      (The script will detect the bootloader automatically)"
echo ""

if [ -e /dev/ttyACM0 ]; then
  echo "      Waiting for reset (port /dev/ttyACM0 to disappear)..."
  timeout=15
  while [ -e /dev/ttyACM0 ] && [ $timeout -gt 0 ]; do
    sleep 0.3
    timeout=$((timeout - 1))
  done
fi

echo "      Waiting for bootloader port..."
found_port=""
for i in $(seq 1 60); do
  for port in /dev/ttyACM* /dev/ttyUSB*; do
    if [ -e "$port" ]; then
      found_port="$port"
      break 2
    fi
  done
  sleep 0.5
done

if [ -z "$found_port" ]; then
  echo "ERROR: No port appeared after 30 seconds. Is the Arduino plugged in?"
  exit 1
fi

echo "      Bootloader detected on $found_port — uploading..."
arduino-cli upload -p "$found_port" --fqbn "$FQBN" "$SKETCH"
echo "      Upload complete"

# ── Step 5: Install BT Scoreboard dependencies ───────────────────────────────
echo ""
echo "[5/6] Installing Bluetooth dependencies..."
sudo apt-get install -y python3-dbus python3-gi bluez -q
sudo sed -i 's/^#*DiscoverableTimeout\s*=.*/DiscoverableTimeout = 0/' /etc/bluetooth/main.conf
echo "      Dependencies installed"

# ── Step 6: Install BT Scoreboard service ────────────────────────────────────
echo "[6/6] Installing BT Scoreboard service..."
sudo mkdir -p /opt/btscoreboard
sudo cp /tmp/btscoreboard.py /opt/btscoreboard/btscoreboard.py
sudo chmod 755 /opt/btscoreboard/btscoreboard.py
sudo cp /tmp/btscoreboard.service /etc/systemd/system/btscoreboard.service
sudo systemctl daemon-reload
sudo systemctl enable btscoreboard
sudo systemctl restart btscoreboard
echo "      BT Scoreboard service installed and started"

echo ""
echo "=== Setup complete! ==="
echo ""
echo "  Arduino sketch : uploaded"
echo "  BT Scoreboard  : running as 'BT-Scoreboard'"
echo "  Logs           : sudo journalctl -u btscoreboard -f"
echo ""
echo "  NOTE: If your admin token is not 'changeme', update it:"
echo "  sudo nano /etc/systemd/system/btscoreboard.service"
echo "  Change ADMIN_TOKEN=changeme then:"
echo "  sudo systemctl daemon-reload && sudo systemctl restart btscoreboard"
