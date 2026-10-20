#!/usr/bin/env pwsh
# Exercise original Mac CDs, the GOG installer, mixed-set cleanup and D1-in-D2
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$Output = 'temp/mac-edition-support/device',
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
& $Python (Join-Path $PSScriptRoot 'run_mac_edition_assets.py') --serial $env:ANDROID_SERIAL --package $script:PACKAGE --adb $ADB --output $Output
if ($LASTEXITCODE -ne 0) { throw 'Mac edition integration failed' }
