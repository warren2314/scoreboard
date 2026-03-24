<?php
	include "php_serial.class.php";

	$serial = new phpSerial;
	$serial->deviceSet("/dev/ttyACM0");
	#$serial->deviceSet("/dev/ttyAMA0");
	#$serial->confBaudRate(115200);
	$serial->confParity("none");
	$serial->confCharacterLength(8);
	$serial->confStopBits(1);
	$tempString="5#";
	#echo($tempString."<br>");
	$serial->deviceOpen();
	$serial->sendMessage($tempString);


	$serial->deviceClose();

    echo("Test mode engaged.");

	#print_r($_POST);
?>
