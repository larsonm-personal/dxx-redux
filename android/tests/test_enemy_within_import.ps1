#!/usr/bin/env pwsh
param([string]$Serial)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$source = Join-Path $PSScriptRoot '../../game_data/mission_files/ewithin-versions.zip'
if (-not (Test-Path -LiteralPath $source)) { throw "Required full Enemy Within archive missing: $source" }
$remote = '/data/local/tmp/dxx-enemy-within-import.zip'
$fixture = 'files/enemy-within-import-fixture.zip'
try {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Adb -Seconds 300 -AdbArgs @('push', $source, $remote) | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files') | Out-Null
    Adb -Seconds 180 -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', $remote, $fixture) | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', $remote) | Out-Null
    $result = Adb -Seconds 600 -AdbArgs @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'enemy_within_import',
        "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation")
    Write-Output $result
    if ($result -notmatch 'PASS: Enemy Within full wrapper' -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'Enemy Within full wrapper import failed'
    }
} finally {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', $fixture) | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-f', $remote) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
