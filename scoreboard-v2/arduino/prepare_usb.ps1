# Droylsden Cricket Club - Prepare USB Flash Drive
# Run this at home to copy everything needed onto the USB stick.

$ARDUINO_CLI_SRC = "C:\Users\warre\Downloads\arduino-cli_1.4.1_Windows_64bit\arduino-cli.exe"
$SKETCH_SRC      = Join-Path $PSScriptRoot "single_digit_test"
$FLASH_PS1_SRC   = Join-Path $PSScriptRoot "flash.ps1"

Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "  Droylsden CC - Prepare USB Drive" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""

# List drives for the user to pick from
$drives = Get-PSDrive -PSProvider FileSystem | Where-Object {
    $_.Root -match '^[A-Z]:\\$' -and $_.Root -ne 'C:\'
} | ForEach-Object {
    $vol = Get-Volume -DriveLetter $_.Name -ErrorAction SilentlyContinue
    $label = if ($vol -and $vol.FileSystemLabel) { $vol.FileSystemLabel } else { "no label" }
    $size  = if ($vol -and $vol.Size -gt 0) { [math]::Round($vol.Size / 1GB, 1).ToString() + " GB" } else { "unknown size" }
    [PSCustomObject]@{
        Letter = $_.Name
        Label  = $label
        Size   = $size
    }
}

if (-not $drives) {
    Write-Host "No drives found other than C:. Plug in your USB stick and try again." -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "Available drives:" -ForegroundColor Yellow
foreach ($d in $drives) {
    Write-Host "  $($d.Letter): $($d.Label) ($($d.Size))"
}

Write-Host ""
$driveLetter = Read-Host "Enter drive letter for your USB (e.g. E)"
$driveLetter = $driveLetter.Trim().TrimEnd(':').ToUpper()
$usbRoot = "${driveLetter}:\DroylsdenScoreboard"

Write-Host ""
Write-Host "Copying files to: $usbRoot" -ForegroundColor Yellow
Write-Host ""

New-Item -ItemType Directory -Force -Path $usbRoot | Out-Null

# Copy arduino-cli
Write-Host "Copying arduino-cli.exe..." -ForegroundColor Cyan
if (-not (Test-Path $ARDUINO_CLI_SRC)) {
    Write-Host "ERROR: arduino-cli not found at $ARDUINO_CLI_SRC" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}
Copy-Item $ARDUINO_CLI_SRC "$usbRoot\arduino-cli.exe" -Force

# Copy sketch
Write-Host "Copying sketch..." -ForegroundColor Cyan
if (-not (Test-Path $SKETCH_SRC)) {
    Write-Host "ERROR: Sketch not found at $SKETCH_SRC" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}
if (Test-Path "$usbRoot\single_digit_test") {
    Remove-Item "$usbRoot\single_digit_test" -Recurse -Force
}
Copy-Item $SKETCH_SRC "$usbRoot\single_digit_test" -Recurse -Force

# Copy flash.ps1 and update it to use local arduino-cli
Write-Host "Copying flash script..." -ForegroundColor Cyan
$flashContent = Get-Content $FLASH_PS1_SRC -Raw
$flashContent = $flashContent -replace '(?m)^\$ARDUINO_CLI\s*=.*$', '$ARDUINO_CLI = Join-Path $PSScriptRoot "arduino-cli.exe"'
[System.IO.File]::WriteAllText("$usbRoot\flash.ps1", $flashContent)

# Write flash.bat
$batContent = "@echo off`r`necho Droylsden Cricket Club - Arduino Flash Tool`r`necho.`r`npowershell.exe -ExecutionPolicy Bypass -File `"%~dp0flash.ps1`"`r`npause`r`n"
[System.IO.File]::WriteAllText("$usbRoot\flash.bat", $batContent)

Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "  USB READY!" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""
Write-Host "Files on USB: $usbRoot" -ForegroundColor Green
Write-Host ""
Write-Host "At the club:" -ForegroundColor Cyan
Write-Host "  1. Plug USB into the PC"
Write-Host "  2. Plug Arduino into the PC via USB"
Write-Host "  3. Open DroylsdenScoreboard folder on the USB"
Write-Host "  4. Double-click flash.bat"
Write-Host "  5. Done!"
Write-Host ""
Read-Host "Press Enter to exit"
