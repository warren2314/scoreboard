# Droylsden Cricket Club - Arduino Flash Script
# Run this from a USB drive to compile and flash the scoreboard sketch
# Requires arduino-cli to be installed on the PC

$ARDUINO_CLI = "C:\Users\warre\Downloads\arduino-cli_1.4.1_Windows_64bit\arduino-cli.exe"
$FQBN        = "arduino:renesas_uno:unor4wifi"
$SKETCH      = Join-Path $PSScriptRoot "single_digit_test"

Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "  Droylsden CC - Arduino Flash Tool" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""

# Check arduino-cli exists
if (-not (Test-Path $ARDUINO_CLI)) {
    Write-Host "ERROR: arduino-cli not found at:" -ForegroundColor Red
    Write-Host "  $ARDUINO_CLI" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please install arduino-cli or update the path in this script." -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

# Check sketch exists
if (-not (Test-Path $SKETCH)) {
    Write-Host "ERROR: Sketch folder not found at:" -ForegroundColor Red
    Write-Host "  $SKETCH" -ForegroundColor Red
    Write-Host "Make sure single_digit_test folder is next to this script on the USB." -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

# Auto-detect COM port for Arduino UNO R4 WiFi
Write-Host "Detecting Arduino..." -ForegroundColor Yellow
$ports = Get-PnpDevice -Class Ports -Status OK | Where-Object {
    $_.FriendlyName -match "Arduino" -or $_.FriendlyName -match "UNO R4" -or $_.FriendlyName -match "USB Serial"
}

$comPort = $null

if ($ports) {
    # Extract COM port number from friendly name
    $match = $ports[0].FriendlyName | Select-String -Pattern "COM\d+"
    if ($match) {
        $comPort = $match.Matches[0].Value
        Write-Host "Found: $($ports[0].FriendlyName)" -ForegroundColor Green
        Write-Host "Using port: $comPort" -ForegroundColor Green
    }
}

if (-not $comPort) {
    Write-Host "Could not auto-detect Arduino port." -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Available COM ports:" -ForegroundColor Cyan
    Get-PnpDevice -Class Ports -Status OK | ForEach-Object {
        Write-Host "  $($_.FriendlyName)"
    }
    Write-Host ""
    $comPort = Read-Host "Enter COM port manually (e.g. COM3)"
    if (-not $comPort) {
        Write-Host "No port entered. Exiting." -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "Sketch:  $SKETCH" -ForegroundColor Cyan
Write-Host "Port:    $comPort" -ForegroundColor Cyan
Write-Host "Board:   $FQBN" -ForegroundColor Cyan
Write-Host ""

# Compile
Write-Host "Compiling..." -ForegroundColor Yellow
& $ARDUINO_CLI compile --fqbn $FQBN $SKETCH
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "COMPILE FAILED. Check errors above." -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""
Write-Host "Compile OK." -ForegroundColor Green
Write-Host ""

# Upload
Write-Host "Uploading to $comPort..." -ForegroundColor Yellow
& $ARDUINO_CLI upload -p $comPort --fqbn $FQBN $SKETCH
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "UPLOAD FAILED. Check errors above." -ForegroundColor Red
    Write-Host ""
    Write-Host "Tips:" -ForegroundColor Yellow
    Write-Host "  - Make sure the Arduino is plugged in via USB"
    Write-Host "  - Try a different USB cable"
    Write-Host "  - Check the COM port is correct in Device Manager"
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "  FLASH SUCCESSFUL!" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""
Write-Host "The Arduino should now show an 8 on digit 0." -ForegroundColor Cyan
Write-Host "Connect to Serial Monitor at 57600 baud to test." -ForegroundColor Cyan
Write-Host ""
Read-Host "Press Enter to exit"
