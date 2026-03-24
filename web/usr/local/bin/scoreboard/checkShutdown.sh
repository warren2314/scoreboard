#!/bin/bash
if [ -f /var/www/shutdown/shutdown.server ]; then
  rm -f /var/www/shutdown/shutdown.server
  /sbin/shutdown now 
fi
