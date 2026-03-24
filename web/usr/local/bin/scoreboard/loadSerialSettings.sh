#! /bin/bash

RETVAL=1
         while [  $RETVAL -ne 0 ]; do
		stty < /dev/ttyACM0 `cat /usr/local/bin/scoreboard/ttyACM0.settings`
		RETVAL=$?
         done
