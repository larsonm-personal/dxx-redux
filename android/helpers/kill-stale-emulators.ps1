#!/usr/bin/env pwsh
# Inspect emulator state; cleanup requires an explicit target serial
# Exit codes: 0 healthy/absent, 1 unhealthy or cleanup failed, 2 cleaned
param([switch]$Kill, [string]$Serial = $env:ANDROID_SERIAL)

$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot "test_helpers.ps1")
if (-not $Serial) {
    if ($Kill) { Write-Host "Specify -Serial to clean up one emulator"; exit 1 }
    $devices = Adb-Timeout -AdbArgs @("devices") -Seconds 5
    $unhealthy = $false
    foreach ($match in [regex]::Matches($devices, '(?m)^(emulator-\d+)\s')) {
        $deviceSerial = $match.Groups[1].Value
        $healthy = Test-EmulatorHealthy -Serial $deviceSerial
        Write-Host "${deviceSerial}: $(if ($healthy) { 'healthy' } else { 'unhealthy' })"
        if (-not $healthy) { $unhealthy = $true }
    }
    if ($unhealthy) { exit 1 }
    exit 0
}
Get-ManagedEmulatorPort -Serial $Serial | Out-Null
if (Test-EmulatorHealthy -Serial $Serial) {
    Write-Host "HEALTHY ($Serial)"
    exit 0
}
$devices = Adb-Timeout -AdbArgs @("devices") -Seconds 5
if ($devices -notmatch "(?m)^$([regex]::Escape($Serial))\s" -and @(Get-ManagedEmulatorProcesses -Serial $Serial).Count -eq 0) {
    Write-Host "ABSENT ($Serial)"
    exit 0
}
if ($Kill -and (Stop-ManagedEmulator -Serial $Serial)) {
    Write-Host "CLEANED ($Serial)"
    exit 2
}
Write-Host "UNHEALTHY ($Serial)"
exit 1
