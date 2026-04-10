# Droylsden Cricket Club - LED Scoreboard

Raspberry Pi 5 web panel → Arduino UNO R4 WiFi → TPIC6B595 shift registers → 7-segment LED displays.

**18 digits total, 3 chains of 6 digits.**

---

## Hardware

| Component | Notes |
|-----------|-------|
| Raspberry Pi 5 | Runs Node.js web server, systemd service `scoreboard` |
| Arduino UNO R4 WiFi | Shows up as `/dev/ttyACM0` on the Pi, `COM3` on Windows |
| TPIC6B595 boards | W19Design "Shift2" boards, 6 daisy-chained per pin set |
| Power | Two separate 5V feeds per chain to avoid voltage drop |

### Arduino pin map (confirmed working)

| Chain | SRCK | SERIN | RCK | Digits |
|-------|------|-------|-----|--------|
| 1 | D2 | D3 | D4 | BatA(3) + Wickets(1) + Overs(2) |
| 2 | D5 | D6 | D7 | Total(3) + BatB(3) |
| 3 | D8 | D9 | D10 | Target(3) + DLS(3) |

**Important:** Pin order is SRCK-SERIN-RCK. Previous versions had SRCK/RCK swapped — only clocked one bit per command. Do not change.

### Voltage drop warning

With 6 boards per chain you MUST run two separate 5V feeds to each chain — one at board 0, one at board 3 or 4. A single feed causes the last boards to receive corrupted data once enough segments are lit. Both feeds share the same PSU and the same GND.

---

## Flashing the Arduino at the club

The Pi 5 cannot flash the R4 WiFi (bossac fails on the Pi 5's RP1 USB controller). You must flash from a Windows PC using `arduino-cli`.

### One-time: prepare the USB stick (at home)

1. Plug in a USB stick
2. Double-click `scoreboard-v2/arduino/prepare_usb.bat`
3. Enter the drive letter for the USB when prompted
4. It copies `arduino-cli.exe`, the `scoreboard` sketch, `flash.ps1`, and `flash.bat` into `<USB>:\DroylsdenScoreboard\`

### At the club: flash the Arduino

1. Plug USB stick into the club PC
2. Plug Arduino into the club PC via USB cable
3. Open `DroylsdenScoreboard` folder on the USB
4. Double-click `flash.bat`
5. It auto-detects the COM port, compiles, uploads, done.

### After flashing

- Send `5#` from the web panel Test button — all 18 digits should show 8
- If any chain fails, check:
  - Power is reaching both ends of that chain (voltage drop)
  - Data wire connections (SERIN from previous board's SEROUT)
  - GND is shared between PSU and Arduino

---

## SD card corruption protection (CRITICAL)

**The scorers will not have access to the Pi and may just yank the power.** You MUST enable the read-only overlay filesystem before handing over, or the SD card will eventually corrupt.

### Enable overlay filesystem (do this ONCE before deployment)

SSH into the Pi:
```bash
sudo raspi-config
```

Navigate to:
- **Performance Options → Overlay File System**
- "Would you like the overlay file system to be enabled?" → **Yes**
- "Would you like the boot partition to be write-protected?" → **Yes**
- Reboot when prompted

Now the root filesystem is read-only. Pulling the plug is safe. All runtime writes go to RAM and are discarded on reboot.

### Disable overlay temporarily (to update code)

When you need to update the server code or flash a new Arduino sketch:

```bash
sudo raspi-config
# Performance Options → Overlay File System → Disable
sudo reboot
```

Make your changes:
```bash
cd /path/to/scoreboard-v2
git pull
sudo systemctl restart scoreboard
```

Test that everything works, then re-enable overlay:
```bash
sudo raspi-config
# Performance Options → Overlay File System → Enable
sudo reboot
```

### What is NOT persisted when overlay is on

- Logs (journald is in RAM)
- Any files written to disk
- systemd state changes

Score state is held in RAM in the Node server — this is fine because the scorers enter scores via the web panel each match anyway. There is nothing to persist between reboots.

---

## Deploying server updates

When code changes need to go onto the Pi:

1. **Disable overlay FS** (see above) and reboot
2. On the Pi:
   ```bash
   cd /path/to/scoreboard-v2
   git pull
   sudo systemctl restart scoreboard
   sudo systemctl status scoreboard
   ```
3. Test the web panel and any Arduino changes
4. **Re-enable overlay FS** and reboot

---

## Web panel

- **Main scorer page:** `http://<pi-ip>/`  — used during matches
- **Admin page:** `http://<pi-ip>/admin`  — Play Cricket sync, reboot, shutdown, test mode
- **Digit test page:** `http://<pi-ip>/digit-test`  — low-level digit-by-digit testing (uses test sketch, not production)

### Play Cricket integration

Admin page → Play Cricket Live Sync section:
- Enter Match ID and API token
- **Tick "Dry Run"** to test parsing without updating the scoreboard (shows what it *would* have sent — use this during a live match to verify before committing)
- Click Start Sync
- Status box shows last update time and parsed score (Bat A / Total / Bat B / etc.)
- Blank positions show as underscores (`_`) so they are obvious

### Score command format (Arduino)

```
4,<batA(3)>,<total(3)>,<batB(3)>,<target(3)>,<wkts(1)>,<overs(2)>,<dls(3)>#
```

Example: `4,--5,123,--8,--0,3,12,--0#`

Other commands: `5#` (test mode, all 8s), `clear#`, `status#`

---

## Troubleshooting

### "Arduino disconnected" on web panel
- Check USB cable from Pi to Arduino
- Check `/dev/ttyACM0` exists on the Pi: `ls /dev/ttyACM*`
- Restart service: `sudo systemctl restart scoreboard`

### Some digits show wrong segments / garbled
- Almost always voltage drop — check both 5V feeds are connected to each chain
- Check the last board in the chain is receiving 5V (measure with multimeter)

### Flash fails on Windows
- Try a different USB cable (some data-only cables don't carry enough power for flashing)
- Check Device Manager for COM port — update `flash.ps1` if COM3 is wrong
- Board might be in bootloader mode — double-tap the reset button on the Arduino

### Pi won't boot after power loss
- Shouldn't happen with overlay FS enabled. If it does, the overlay may not have been enabled properly. Reimage the SD card and re-enable overlay before handing over again.

---

## File locations reference

| What | Where |
|------|-------|
| Production Arduino sketch | `arduino/scoreboard/scoreboard.ino` |
| Test sketch (digit-by-digit) | `arduino/single_digit_test/single_digit_test.ino` |
| Flash USB prep script | `arduino/prepare_usb.bat` |
| Flash script (on USB) | `<USB>:\DroylsdenScoreboard\flash.bat` |
| Node server | `server/server.js` |
| Scorer web panel | `server/public/index.html` + `server/public/js/app.js` |
| Admin web panel | `server/public/admin.html` |
| systemd service | `scoreboard.service` |
