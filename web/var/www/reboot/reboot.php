<?php
	$fh = fopen("/var/www/reboot/reboot.server",'w');
	fwrite($fh,"Reboot now\n");
	fclose($fh);
	echo("Scoreboard will reboot in the next minute.  Please Wait.");
?>
