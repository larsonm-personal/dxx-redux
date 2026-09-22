#!/usr/bin/env pwsh
# Catalog owner for the isolated Android D1-only resource/gameplay scenario
param(
    [string]$D1DataDirectory,
    [string]$Serial = 'emulator-5554',
    [string]$AdbPath = 'C:\local\android-sdk\platform-tools\adb.exe'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $D1DataDirectory) {
    $demo = Join-Path $repoRoot 'android/regression_demos/d1_descent_level14_20260920_184354.dximdemo'
    $resolved = & (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1') `
        -DemoPath $demo -Game d1 -Mode accelerated -ResolveDataDirOnly
    if ($LASTEXITCODE -ne 0) { throw 'Could not resolve native D1 resources' }
    $line = @($resolved | Where-Object { $_ -like 'Resolved d1 data dir: *' })
    if ($line.Count -ne 1) { throw 'Native data resolver returned no unique directory' }
    $D1DataDirectory = $line[0].Substring('Resolved d1 data dir: '.Length)
}
& (Join-Path $repoRoot 'android/helpers/test_d1_in_d2_android.ps1') `
    -D1DataDirectory $D1DataDirectory -Serial $Serial -AdbPath $AdbPath
