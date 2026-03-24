#!/bin/bash
if [ -f /var/www/reboot/reboot.server ]; then
  rm -f /var/www/reboot/reboot.server
  /sbin/shutdown -r now 
fi
