<?php
error_reporting(E_ALL);
ini_set('display_errors', '1');
include "php_serial.class.php";
 
$serial = new phpSerial;
$serial->deviceSet("/dev/ttyACM0");
#$serial->deviceSet("/dev/ttyAMA0");
#$serial->confBaudRate(115200);
$serial->confBaudRate(57600);
$serial->confParity("none");
$serial->confCharacterLength(8);
$serial->confStopBits(1);
$serial->deviceOpen();
$serial->sendMessage('5#');

$serial->deviceClose();
 
echo "I've sended a message! \n\r";

?>
