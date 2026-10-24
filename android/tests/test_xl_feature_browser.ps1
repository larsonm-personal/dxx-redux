#!/usr/bin/env pwsh
param([string]$Serial)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$output = Join-Path $PSScriptRoot '../temp/xl-feature-browser'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Path $output -Force | Out-Null
try {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    $result = Adb -Seconds 90 -AdbArgs @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'xl_compatibility',
        "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation")
    Write-Output $result
    if ($result -notmatch 'PASS: XL compatibility dialog text and colors' -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'XL compatibility dialog instrumentation failed'
    }
} finally {
    foreach ($theme in @('browser-light', 'browser-dark', 'water-light', 'water-dark')) {
        try { Adb -AdbArgs @('pull', "/sdcard/Android/data/$($script:PACKAGE)/files/xl-feature-$theme.png", $output) | Out-Null }
        catch { Write-Warning "No $theme screenshot: $_" }
    }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
