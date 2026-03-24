#!/bin/bash
#
# Deploy cricket scoreboard to Raspberry Pi 3 B+
#
# Usage:
#   ./deploy_to_pi.sh <pi-ip-address> [pi-username]
#
# Examples:
#   ./deploy_to_pi.sh 192.168.1.50
#   ./deploy_to_pi.sh 192.168.1.50 pi
#
# Prerequisites:
#   - Pi is on the network with SSH enabled
#   - You know the Pi's IP address (check your router or run: hostname -I on the Pi)
#   - Arduino is plugged into the Pi via USB

set -e

PI_HOST="${1:?Usage: $0 <pi-ip-address> [pi-username]}"
PI_USER="${2:-pi}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Cricket Scoreboard Deployment ==="
echo "Target: ${PI_USER}@${PI_HOST}"
echo ""

# Step 1: Copy the tarball to the Pi
echo "[1/4] Copying raspberry.tgz to Pi..."
scp "${SCRIPT_DIR}/raspberry.tgz" "${PI_USER}@${PI_HOST}:/tmp/raspberry.tgz"

# Step 2: Run the setup on the Pi over SSH
echo "[2/4] Installing packages and deploying files..."
ssh "${PI_USER}@${PI_HOST}" bash -s <<'REMOTE_SCRIPT'
set -e

echo "--- Updating package list ---"
sudo apt-get update -qq

echo "--- Installing Apache2, PHP, and dependencies ---"
sudo apt-get install -y apache2 php libapache2-mod-php cron

echo "--- Extracting scoreboard files to filesystem root ---"
cd /
sudo tar xzf /tmp/raspberry.tgz
rm /tmp/raspberry.tgz

echo "--- Setting web file ownership ---"
sudo chown -R www-data:www-data /var/www

echo "--- Setting script permissions ---"
sudo chmod +x /usr/local/bin/scoreboard/*.sh

echo "--- Enabling Apache mod_rewrite and mod_authn_file (for .htaccess auth) ---"
sudo a2enmod rewrite
sudo a2enmod authn_file

echo "--- Configuring Apache to allow .htaccess overrides ---"
# Update the default site to allow .htaccess in /var/www
if ! grep -q "AllowOverride All" /etc/apache2/sites-available/000-default.conf 2>/dev/null; then
    sudo bash -c 'cat >> /etc/apache2/sites-available/000-default.conf <<EOF

<Directory /var/www>
    AllowOverride All
    Require all granted
</Directory>
EOF'
fi

echo "--- Adding www-data to dialout group (serial port access) ---"
sudo usermod -a -G dialout www-data

echo "--- Setting up sudo permissions for shutdown/reboot ---"
sudo bash -c 'cat > /etc/sudoers.d/scoreboard <<EOF
# Allow cron scripts to shutdown/reboot without password
root ALL=(ALL) NOPASSWD: /sbin/shutdown
www-data ALL=(ALL) NOPASSWD: /sbin/shutdown
EOF'
sudo chmod 440 /etc/sudoers.d/scoreboard

echo "--- Setting up cron jobs ---"
# Install crontab for root (reboot/shutdown checks + serial setup)
(sudo crontab -l 2>/dev/null | grep -v scoreboard; cat <<'CRON'
# Scoreboard: check for reboot/shutdown requests every minute
* * * * * /usr/local/bin/scoreboard/checkReboot.sh
* * * * * /usr/local/bin/scoreboard/checkShutdown.sh
# Scoreboard: load serial settings at boot
@reboot /usr/local/bin/scoreboard/loadSerialSettings.sh
# Scoreboard: start serial logging at boot
@reboot /usr/local/bin/scoreboard/readFromSerial.sh &
# Scoreboard: clean log at boot
@reboot /usr/local/bin/scoreboard/cleanup.sh
CRON
) | sudo crontab -

echo "--- Restarting Apache ---"
sudo systemctl restart apache2
sudo systemctl enable apache2

echo ""
echo "=== Deployment complete! ==="
REMOTE_SCRIPT

# Step 3: Verify it's running
echo ""
echo "[3/4] Verifying Apache is serving the scoreboard..."
if curl -s --connect-timeout 5 "http://${PI_HOST}/index.htm" | grep -qi "scoreboard"; then
    echo "SUCCESS: Scoreboard web UI is accessible at http://${PI_HOST}/index.htm"
else
    echo "WARNING: Could not verify scoreboard page. Check http://${PI_HOST}/ manually."
fi

echo ""
echo "[4/4] Done!"
echo ""
echo "========================================"
echo "  Next steps:"
echo "  1. Plug the Arduino into the Pi via USB"
echo "  2. Reboot the Pi: ssh ${PI_USER}@${PI_HOST} sudo reboot"
echo "  3. Open http://${PI_HOST}/index.htm on your phone"
echo "  4. Admin panel: http://${PI_HOST}/admin.htm"
echo "========================================"
