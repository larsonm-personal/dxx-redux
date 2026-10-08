#!/usr/bin/env pwsh
param([string]$Serial)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
Ensure-EmulatorHealthy | Out-Null
$oraclePath = "$PSScriptRoot/fixtures/demo_import_oracles.json"
$oracles = Get-Content -LiteralPath $oraclePath -Raw | ConvertFrom-Json
$sourceRoot = Join-Path $PSScriptRoot '../../game_data/demo installers'
$remote = '/data/local/tmp/dxx-demo-import'
try {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Adb -AdbArgs @('shell', 'mkdir', '-p', $remote) | Out-Null
    Adb -AdbArgs @('push', $oraclePath, "$remote/oracles.json") | Out-Null
    for ($i = 0; $i -lt $oracles.archives.Count; $i++) {
        $source = Join-Path $sourceRoot $oracles.archives[$i].archive
        if (-not (Test-Path -LiteralPath $source)) { throw "Required original demo fixture missing: $source" }
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $oracles.archives[$i].sha256) {
            throw "Original demo fixture hash mismatch: $source"
        }
        Adb -Seconds 120 -AdbArgs @('push', $source, "$remote/archive-$i") | Out-Null
    }
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', 'files/demo-import-fixtures') | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '-r', "$remote/.", 'files/demo-import-fixtures/') | Out-Null
    $result = Adb -Seconds 600 -AdbArgs @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'demo_import',
        "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation")
    Write-Output $result
    if ($result -notmatch 'PASS: demo corpus import' -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'Demo production import instrumentation failed'
    }
} finally {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-rf', 'files/demo-import-fixtures') | Out-Null
    Adb -AdbArgs @('shell', 'rm', '-rf', $remote) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
