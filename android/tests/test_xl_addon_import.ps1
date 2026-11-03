#!/usr/bin/env pwsh
param([string]$Serial)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
try {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    $result = Adb -Seconds 180 -AdbArgs @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'xl_addon_import',
        "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation")
    Write-Output $result
    if ($result -notmatch 'PASS: XL addon selective import' -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'XL addon import checks failed'
    }
} finally {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
