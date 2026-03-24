<?php

	$totalHundreds=$_POST[totalHundreds];	
	$totalTens=$_POST[totalTens];	
	$totalOnes=$_POST[totalOnes];	
	$wicketsOnes=$_POST[wicketsOnes];	
	$oversTens=$_POST[oversTens];	
	$oversOnes=$_POST[oversOnes];	
	$batsmanaHundreds=$_POST[batsmanaHundreds];	
	$batsmanaTens=$_POST[batsmanaTens];	
	$batsmanaOnes=$_POST[batsmanaOnes];	
	$batsmanbHundreds=$_POST[batsmanbHundreds];	
	$batsmanbTens=$_POST[batsmanbTens];	
	$batsmanbOnes=$_POST[batsmanbOnes];	
	$targetHundreds=$_POST[targetHundreds];	
	$targetTens=$_POST[targetTens];	
	$targetOnes=$_POST[targetOnes];	

	$total=$totalHundreds.$totalTens.$totalOnes;
	$overs=$oversTens.$oversOnes;
	$batsmana=$batsmanaHundreds.$batsmanaTens.$batsmanaOnes;
	$batsmanb=$batsmanbHundreds.$batsmanbTens.$batsmanbOnes;
	$target=$targetHundreds.$targetTens.$targetOnes;

    echo("Total: " . $total . " for " . $wicketsOnes . " wickets from " . $overs . " overs. Target: " . $target . ". Batsman A is on: " . $batsmana . " Batsman B is on: " .$batsmanb);

	#print_r($_POST);
?>
