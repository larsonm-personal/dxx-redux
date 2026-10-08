#!/usr/bin/env pwsh
# Import the initial MacPlay disc and verify actual D1 effects output
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$Output = 'temp/macplay-audio',
    [string]$Python = 'python'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
& $Python (Join-Path $PSScriptRoot 'measure_macplay_audio.py') --serial $env:ANDROID_SERIAL --package $script:PACKAGE --adb $ADB --output $Output --script (Join-Path $PSScriptRoot '../game_scripts/test_macplay_audio.jsonc')
if ($LASTEXITCODE -ne 0) { throw 'MacPlay disc audio integration failed' }
