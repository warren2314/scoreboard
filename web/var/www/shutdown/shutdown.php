<?php
	$fh = fopen("/var/www/shutdown/shutdown.server",'w');
	fwrite($fh,"Shutdown now\n");
	fclose($fh);
	echo("Scoreboard will shutdown in the next minute.  Please Wait.");
?>
