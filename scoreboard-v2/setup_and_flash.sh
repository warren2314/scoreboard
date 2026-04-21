#!/bin/bash
set -e

FQBN="arduino:renesas_uno:unor4wifi"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SKETCH="$SCRIPT_DIR/arduino/scoreboard/scoreboard.ino"
CLI_DIR="$HOME/.local/bin"
CLI="$CLI_DIR/arduino-cli"

echo "=== Droylsden CC Scoreboard Flash Tool ==="
echo ""

# ── Step 0: Pull latest sketch from warren branch ────────────────────────────
echo "[0/4] Fetching latest sketch from warren branch..."
git fetch origin warren 2>/dev/null || true
git show origin/warren:scoreboard-v2/arduino/scoreboard/scoreboard.ino > "$SKETCH"
echo "      Sketch updated from warren branch"
echo ""

# ── Step 1: Install arduino-cli if missing ──────────────────────────────────
if ! command -v arduino-cli &>/dev/null && [ ! -x "$CLI" ]; then
  echo "[1/4] Installing arduino-cli..."
  mkdir -p "$CLI_DIR"
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$CLI_DIR" sh
  export PATH="$CLI_DIR:$PATH"
  echo "      arduino-cli installed to $CLI_DIR"
else
  export PATH="$CLI_DIR:$PATH"
  echo "[1/4] arduino-cli already installed ($(arduino-cli version | head -1))"
fi

# ── Step 2: Install R4 core if missing ──────────────────────────────────────
echo "[2/4] Checking Arduino R4 core..."
if ! arduino-cli core list | grep -q "arduino:renesas_uno"; then
  echo "      Installing arduino:renesas_uno core (this may take a few minutes)..."
  arduino-cli core update-index
  arduino-cli core install arduino:renesas_uno
else
  echo "      Core already installed"
fi

# ── Step 3: Compile ──────────────────────────────────────────────────────────
echo "[3/4] Compiling sketch..."
arduino-cli compile --fqbn "$FQBN" "$SKETCH"
echo "      Compile successful"

# ── Step 4: Upload (wait for bootloader) ─────────────────────────────────────
echo ""
echo "[4/4] Ready to upload"
echo "      >>> Double-tap the RESET button on the Arduino now <<<"
echo "      (The script will detect the bootloader automatically)"
echo ""

# Wait for any existing port to disappear first (confirms reset happened)
if [ -e /dev/ttyACM0 ]; then
  echo "      Waiting for reset (port /dev/ttyACM0 to disappear)..."
  timeout=15
  while [ -e /dev/ttyACM0 ] && [ $timeout -gt 0 ]; do
    sleep 0.3
    timeout=$((timeout - 1))
  done
fi

# Now wait for bootloader port to appear
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

echo ""
echo "=== Upload complete! ==="
