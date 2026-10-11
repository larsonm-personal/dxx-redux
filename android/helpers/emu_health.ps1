#!/usr/bin/env pwsh
# Check or recover one emulator using the shared provisioning and bounded ADB helpers
# Exit codes: 0 healthy, 1 failed, 2 recovered
param(
    [switch]$Restart,
    [switch]$Wait,
    [int]$TimeoutSeconds = 120,
    [string]$AvdName,
    [string]$GpuRenderer = "host",
    [switch]$Headless,
    [switch]$SoftwareCpu = ($env:DXX_ANDROID_SOFTWARE_EMULATOR -eq "1"),
    [string]$PreferredSerial = $env:ANDROID_SERIAL,
    [switch]$ForceRestart
)

if ($SoftwareCpu -and -not $PSBoundParameters.ContainsKey('TimeoutSeconds')) { $TimeoutSeconds = 1200 }

$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot "test_helpers.ps1")
if (-not $PreferredSerial) { $PreferredSerial = $script:PRIMARY_EMULATOR_SERIAL }
Get-ManagedEmulatorPort -Serial $PreferredSerial | Out-Null

if (-not ($Restart -and $ForceRestart) -and (Test-EmulatorHealthy -Serial $PreferredSerial)) {
    Write-Host "HEALTHY ($PreferredSerial)"
    exit 0
}
if (-not $Restart) {
    if ($Wait -and (Wait-EmulatorBootComplete -Serial $PreferredSerial -TimeoutSeconds $TimeoutSeconds -AdbTimeoutSeconds $(if ($SoftwareCpu) { 30 } else { 5 }) -RecoverSharedStorage:$SoftwareCpu)) { exit 0 }
    Write-Host "UNHEALTHY ($PreferredSerial)"
    exit 1
}

if (-not $AvdName) {
    $avd = Adb-Dev-Timeout -Serial $PreferredSerial -AdbArgs @("emu", "avd", "name") -Seconds 5
    if ($avd) { $AvdName = ($avd -split "`n" | Where-Object { $_.Trim() -and $_.Trim() -ne "OK" } | Select-Object -First 1).Trim() }
    if (-not $AvdName) {
        switch ($PreferredSerial) {
            $script:PRIMARY_EMULATOR_SERIAL { $AvdName = $script:PRIMARY_AVD_NAME }
            $script:SECONDARY_EMULATOR_SERIAL { $AvdName = $script:SECONDARY_AVD_NAME }
            default { Write-Host "Supply -AvdName to recover $PreferredSerial"; exit 1 }
        }
    }
}
if (-not (Ensure-ManagedAvdExists -AvdName $AvdName)) { exit 1 }
if (-not (Stop-ManagedEmulator -Serial $PreferredSerial)) {
    Write-Host "Could not stop $PreferredSerial"
    exit 1
}
if (Start-ManagedEmulator -AvdName $AvdName -Serial $PreferredSerial -GpuRenderer $GpuRenderer -Headless:$Headless -SoftwareCpu:$SoftwareCpu -BootTimeoutSeconds $TimeoutSeconds) {
    Write-Host "RESTARTED_AND_HEALTHY ($PreferredSerial)"
    exit 2
}
exit 1
