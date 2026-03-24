 #!/bin/bash 
         COUNTER=0
         while [  $COUNTER -ne 1 ]; do
		cat /dev/ttyACM0 >> /var/log/scoreboard.log
         done
