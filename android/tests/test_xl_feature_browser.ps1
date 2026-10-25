#!/usr/bin/env pwsh
param([string]$Serial, [string]$DragonPackageArchive)
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
    $instrumentArgs = @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'xl_compatibility')
    if ($DragonPackageArchive) { $instrumentArgs += @('-e', 'dragon_package_archive', $DragonPackageArchive) }
    $instrumentArgs += "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation"
    $result = Adb -Seconds 90 -AdbArgs $instrumentArgs
    Write-Output $result
    $result | Set-Content (Join-Path $output 'instrumentation.txt')
    if ($result -notmatch 'PASS: XL compatibility dialog text and colors' -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'XL compatibility dialog instrumentation failed'
    }
} finally {
    $captures = if ($DragonPackageArchive) { @('dragon-sounds') } else { @('browser-light', 'browser-dark', 'water-light', 'water-dark') }
    foreach ($theme in $captures) {
        try { Adb -AdbArgs @('pull', "/sdcard/Android/data/$($script:PACKAGE)/files/xl-feature-$theme.png", $output) | Out-Null }
        catch { Write-Warning "No $theme screenshot: $_" }
    }
    if ($DragonPackageArchive) {
        try { Adb -AdbArgs @('pull', "/sdcard/Android/data/$($script:PACKAGE)/files/xl-feature-dragon-evidence.txt", $output) | Out-Null }
        catch { Write-Warning "No Dragon Pit evidence file: $_" }
    }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
