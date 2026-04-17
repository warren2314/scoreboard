# Droylsden Cricket Club — LED Scoreboard

## What this is

A Raspberry Pi 5 runs a web server that talks to an Arduino UNO R4 WiFi over USB. The Arduino drives 18 seven-segment LED digits via TPIC6B595 shift registers.

Scores come in two ways:
- **Primary:** Play Cricket Scorer App on a phone/tablet via Bluetooth
- **Backup:** Web scorer panel in any browser on the local network

---

## Physical board layout

```
[ Bat A  ]  [ Total  ]  [ Bat B  ]
[ Target ]  [        ]  [ Wkts   ]
[ DLS    ]  [        ]  [ Overs  ]
```

### Digit index map

| Display | Digits | Chain | Indices |
|---------|--------|-------|---------|
| Bat A   | 3      | 1     | 0, 1, 2 |
| Total   | 3      | 1     | 3, 4, 5 |
| Bat B   | 3      | 2     | 6, 7, 8 |
| Wkts    | 1      | 2     | 9       |
| Overs   | 2      | 2     | 10, 11  |
| Target  | 3      | 3     | 12, 13, 14 |
| DLS     | 3      | 3     | 15, 16, 17 |

### Arduino pin map

| Chain | SRCK | SERIN | RCK | Covers           |
|-------|------|-------|-----|------------------|
| 1     | D2   | D3    | D4  | Bat A + Total    |
| 2     | D5   | D6    | D7  | Bat B + Wkts + Overs |
| 3     | D8   | D9    | D10 | Target + DLS     |

---

## Hardware

| Component | Detail |
|-----------|--------|
| Raspberry Pi 5 | Runs Node.js server + BT scoreboard service |
| Arduino UNO R4 WiFi | `/dev/ttyACM0` on Pi, 57600 baud |
| TPIC6B595 boards | W19Design "Shift2", 6 per chain, 3 chains |
| Power supply | Two separate 5V feeds per chain (one at board 0, one at board 3) to avoid voltage drop |

> **Voltage drop:** With 6 boards per chain you MUST run two 5V feeds into each chain. A single feed causes garbled segments on the last boards when enough LEDs are lit. Both feeds share the same GND.

---

## First time setup (do this once)

### 1. Clone the repo on the Pi

```bash
git clone https://github.com/warren2314/scoreboard.git
cd scoreboard/scoreboard-v2
```

### 2. Run the deploy script

```bash
chmod +x deploy.sh
./deploy.sh
```

This installs:
- Node.js web server (`/opt/scoreboard`)
- BT Scoreboard service (`/opt/btscoreboard`)
- Flashes the Arduino sketch (Arduino must be plugged in)

### 3. Set the admin token

The admin token protects the score API and admin panel. Set the **same token** in both service files:

```bash
sudo nano /etc/systemd/system/scoreboard.service
# Change: ADMIN_TOKEN=changeme → ADMIN_TOKEN=yourtoken

sudo nano /etc/systemd/system/btscoreboard.service
# Change: ADMIN_TOKEN=changeme → yourtoken (must match)

sudo systemctl daemon-reload
sudo systemctl restart scoreboard btscoreboard
```

### 4. Protect the SD card (CRITICAL)

Scorers will just yank the power — enable the read-only overlay filesystem so the SD card doesn't corrupt:

```bash
sudo raspi-config
# Performance Options → Overlay File System → Enable → Yes to both → Reboot
```

Once this is on, pulling the plug is safe. All writes go to RAM and are discarded on reboot.

---

## Updating the code (after first setup)

You need to temporarily disable the overlay filesystem to make changes:

```bash
# 1. Disable overlay and reboot
sudo raspi-config
# Performance Options → Overlay File System → Disable → Reboot

# 2. Pull latest code and redeploy
cd scoreboard/scoreboard-v2
git pull
./deploy.sh

# 3. Test everything works, then re-enable overlay
sudo raspi-config
# Performance Options → Overlay File System → Enable → Reboot
```

---

## Flashing the Arduino

The deploy script flashes automatically if the Arduino is plugged in. If you need to flash manually (e.g. the Pi 5 bossac issue):

**From a Windows PC:**
1. Plug Arduino into PC via USB
2. Run from the repo root:
```
scoreboard-v2\arduino\scoreboard\  (open this folder)
arduino-cli compile -b arduino:renesas_uno:unor4wifi scoreboard-v2/arduino/scoreboard
arduino-cli upload -b arduino:renesas_uno:unor4wifi -p COM3 scoreboard-v2/arduino/scoreboard
```

**Useful Arduino serial commands** (57600 baud, `#` terminated):

| Command | What it does |
|---------|-------------|
| `alltest#` | All 18 digits show 8 |
| `walk#` | Steps an 8 through each digit, prints index |
| `clear#` | All digits off |
| `status#` | Print last known score |
| `digit,9,5#` | Set digit 9 (Wkts) to 5 |

---

## Game day

### Bluetooth (primary)

1. Power on the scoreboard — Pi and Arduino boot automatically
2. Open the **Play Cricket Scorer App** on your phone/tablet
3. Create a new game in the app
4. Go to **Settings → External Scoreboard → Generic**
5. Tap **Not Connected** — it will find **BT-Scoreboard**
6. Tap it to connect — status changes to **Connected**
7. Score as normal — every update goes straight to the board

### Web scorer panel (backup)

If Bluetooth drops or you need to override a score manually:

1. Connect your phone to the same WiFi as the Pi
2. Open `http://<pi-ip>/` in your browser
3. Enter the scorer token
4. Use + / − buttons to update each field
5. Changes send to the board immediately

> The **New Innings** button resets all scores to zero and sends to the board. This is the only way to reset — refreshing the page no longer wipes the score.

---

## Admin panel

Open `http://<pi-ip>/admin` — requires the admin token.

| Button | What it does |
|--------|-------------|
| Test Mode | All 18 digits show 8 |
| Load from USB | Reads `scoreboard.json` from a USB stick |
| Start Sync | Start Play Cricket API sync (backup scoring method) |
| Stop Sync | Stop Play Cricket API sync |
| Reboot Pi | Reboots the Pi |
| Shutdown Pi | Shuts down the Pi safely |

### USB config (for Play Cricket API sync)

If you want to use the API sync rather than Bluetooth, put a file called `scoreboard.json` on a USB stick:

```json
{ "matchId": "4789123", "apiToken": "your-playcricket-api-key" }
```

Plug it in, press **Load from USB**, tick **Dry Run** to test first, then **Start Sync**.

---

## Remote access (Tailscale)

To SSH into the Pi from anywhere (useful if you're not at the ground):

```bash
# On the Pi
curl -fsSL https://tailscale.com/install.sh | sh
sudo tailscale up
```

Install Tailscale on your phone/laptop and sign in with the same account. The Pi gets a permanent IP you can SSH to from anywhere without port forwarding.

---

## Troubleshooting

### Digits don't initialise on power-on
The server sends the current score 2 seconds after the serial port opens. If digits still don't appear, check:
- USB cable from Pi to Arduino is seated properly
- `sudo journalctl -u scoreboard -f` for errors

### Arduino disconnected in web panel
```bash
ls /dev/ttyACM*          # should show /dev/ttyACM0
sudo systemctl restart scoreboard
```

### Garbled or wrong segments on a chain
Almost always voltage drop. Check both 5V feeds are connected to the chain. Measure voltage at the last board — should be above 4.7V.

### Bluetooth not connecting
```bash
sudo systemctl status btscoreboard
sudo journalctl -u btscoreboard -f
```
Make sure the Pi's Bluetooth is on and the Play Cricket Scorer App is set to **Generic** scoreboard type.

### Need to check what Play Cricket API would send
Admin panel → tick **Dry Run** → Start Sync. The status box shows what it parsed every 60 seconds without touching the board. Also visible in logs:
```bash
sudo journalctl -u scoreboard -f
```

### SD card corruption after power loss
Overlay filesystem was not enabled. Reimage, redeploy, and enable overlay before handing back over.

---

## File reference

| File | What it is |
|------|-----------|
| `arduino/scoreboard/scoreboard.ino` | Arduino sketch |
| `arduino/scoreboard/README.md` | Digit positions quick reference |
| `server/server.js` | Node.js API server |
| `server/public/index.html` | Scorer web panel |
| `server/public/admin.html` | Admin panel |
| `btscoreboard/btscoreboard.py` | BLE receiver for Play Cricket Scorer App |
| `btscoreboard/btscoreboard.service` | systemd service for BT scoreboard |
| `scoreboard.service` | systemd service for Node.js server |
| `deploy.sh` | One-command deploy script (run on Pi) |

---

## Logs

```bash
sudo journalctl -u scoreboard -f      # Node.js server
sudo journalctl -u btscoreboard -f    # BT scoreboard
```
