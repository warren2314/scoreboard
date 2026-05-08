#!/bin/bash
#
# Optional field fallback: make the Raspberry Pi broadcast a local WiFi network
# for admin/reset access when no club WiFi or internet is available.
#
# Usage on the Pi:
#   cd scoreboard-v2
#   HOTSPOT_PASSWORD='change-this-password' bash setup_hotspot.sh
#
# Defaults:
#   SSID:     DCC-Scoreboard
#   Password: scoreboard123
#   Admin:    http://10.42.0.1/admin

set -e

SSID="${HOTSPOT_SSID:-DCC-Scoreboard}"
PASSWORD="${HOTSPOT_PASSWORD:-scoreboard123}"
IP_CIDR="${HOTSPOT_IP:-10.42.0.1/24}"

if [ "${#PASSWORD}" -lt 8 ]; then
  echo "ERROR: HOTSPOT_PASSWORD must be at least 8 characters."
  exit 1
fi

if ! command -v nmcli >/dev/null 2>&1; then
  echo "ERROR: nmcli is not installed. This script expects Raspberry Pi OS with NetworkManager."
  echo "Install/configure NetworkManager first, or create the hotspot manually."
  exit 1
fi

echo "Configuring WiFi hotspot:"
echo "  SSID: $SSID"
echo "  IP:   $IP_CIDR"

sudo nmcli connection delete "$SSID" >/dev/null 2>&1 || true
sudo nmcli connection add type wifi ifname wlan0 con-name "$SSID" autoconnect yes ssid "$SSID"
sudo nmcli connection modify "$SSID" \
  802-11-wireless.mode ap \
  802-11-wireless.band bg \
  ipv4.method shared \
  ipv4.addresses "$IP_CIDR" \
  wifi-sec.key-mgmt wpa-psk \
  wifi-sec.psk "$PASSWORD"
sudo nmcli connection up "$SSID"

echo ""
echo "Hotspot is active."
echo "Connect a phone to '$SSID', then open: http://10.42.0.1/admin"
echo "Bluetooth scoring can continue separately through Play Cricket Scorer."
