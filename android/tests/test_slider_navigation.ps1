#!/usr/bin/env pwsh
param([string]$Serial = 'emulator-5554', [switch]$CapabilitiesOnly)

$ErrorActionPreference = 'Stop'
if ($Serial -notmatch '^emulator-\d+$') { throw 'Run this fixture only on an emulator' }
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
try {
    if (-not (Test-DeviceOnline -Serial $Serial)) { throw 'Emulator is not online' }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    $suite = if ($CapabilitiesOnly) { 'graphics_capabilities' } else { 'slider_navigation' }
    $expected = if ($CapabilitiesOnly) { 'PASS: graphics capability controls and details' } else { 'PASS: controller slider navigation and adjustment' }
    $result = Adb -Seconds 300 -AdbArgs @('shell', 'am', 'instrument', '-w', '-e', 'suite', $suite,
        'com.dxxredux.app.test/com.dxxredux.app.RecoveryInstrumentation')
    Write-Output $result
    if ($result -notmatch [regex]::Escape($expected) -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'Controller slider navigation instrumentation failed'
    }
} finally {
    Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-SliderTest:I', 'DXX-NavRepeat:D', 'AndroidRuntime:E') | Write-Output
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
