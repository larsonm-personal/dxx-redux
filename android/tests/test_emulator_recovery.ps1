#!/usr/bin/env pwsh
# Run with two idle test emulators; restart only RestartSerial and preserve GuardSerial
param(
    [string]$RestartSerial = 'emulator-5554',
    [string]$GuardSerial = 'emulator-5556'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. "$PSScriptRoot/../helpers/test_helpers.ps1"
if ($RestartSerial -eq $GuardSerial) { throw 'The recovery and guard devices must differ' }
foreach ($serial in @($RestartSerial, $GuardSerial)) {
    if (-not (Test-EmulatorHealthy -Serial $serial)) { throw "$serial must be healthy before this test" }
}
$marker = '/data/local/tmp/dxx-recovery-' + [guid]::NewGuid().ToString('N')
$bootArgs = @('shell', 'cat', '/proc/sys/kernel/random/boot_id')
$guardBoot = Adb-Dev-Timeout -Serial $GuardSerial -AdbArgs $bootArgs
$restartBoot = Adb-Dev-Timeout -Serial $RestartSerial -AdbArgs $bootArgs
if (-not $guardBoot -or -not $restartBoot) { throw 'Could not read initial boot IDs' }
$previousSerial = $env:ANDROID_SERIAL
try {
    Adb-Dev-Timeout -Serial $GuardSerial -AdbArgs @('shell', 'touch', $marker) | Out-Null
    if ((Adb-Dev-Timeout -Serial $GuardSerial -AdbArgs @('shell', 'ls', $marker)) -ne $marker) {
        throw 'Could not create the guard marker'
    }
    $env:ANDROID_SERIAL = $RestartSerial
    & "$PSScriptRoot/../helpers/emu_health.ps1" -Restart -ForceRestart -Wait -Headless -PreferredSerial $RestartSerial
    if ($LASTEXITCODE -ne 2) { throw 'Selected emulator recovery failed' }
    if ((Adb-Dev-Timeout -Serial $RestartSerial -AdbArgs $bootArgs) -eq $restartBoot) {
        throw 'The requested emulator did not restart'
    }
    if ((Adb-Dev-Timeout -Serial $GuardSerial -AdbArgs $bootArgs) -ne $guardBoot) {
        throw 'Recovery restarted the guard emulator'
    }
    foreach ($serial in @($RestartSerial, $GuardSerial)) {
        if (-not (Test-EmulatorHealthy -Serial $serial)) { throw "$serial became unhealthy" }
    }
    if ((Adb-Dev-Timeout -Serial $GuardSerial -AdbArgs @('shell', 'ls', $marker)) -ne $marker) {
        throw 'Recovery removed unrelated temporary files on the guard device'
    }
    # A missing cleanup target must fail without touching either device
    Remove-Item Env:ANDROID_SERIAL -ErrorAction SilentlyContinue
    & "$PSScriptRoot/../helpers/kill-stale-emulators.ps1" -Kill
    if ($LASTEXITCODE -ne 1) { throw 'Unscoped cleanup was accepted' }
    if (-not (Test-EmulatorHealthy -Serial $RestartSerial) -or -not (Test-EmulatorHealthy -Serial $GuardSerial)) {
        throw 'Unscoped cleanup changed a device'
    }
    Write-Host 'PASS: selected recovery, guard continuity, temporary-file preservation, unscoped cleanup rejection'
} finally {
    foreach ($serial in @($RestartSerial, $GuardSerial)) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'rm', '-f', $marker) | Out-Null
    }
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
exit 0
