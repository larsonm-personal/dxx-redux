#!/usr/bin/env pwsh
# End-to-end asset integration: generate real media or validate a completed run
[CmdletBinding()]
param([string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$androidDir = Split-Path $PSScriptRoot
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $androidDir ('temp/store-assets_test_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
    & (Join-Path $androidDir 'generate-store-assets.ps1') -OutputDirectory $OutputDirectory
    if ($LASTEXITCODE) { throw 'Store asset generation failed' }
}
$python = Join-Path $androidDir $(if ($IsWindows) { 'temp/store-assets-tools/venv/Scripts/python.exe' } else { 'temp/store-assets-tools/venv/bin/python' })
if (!(Test-Path $python)) { $python = (Get-Command python -ErrorAction Stop).Source }
& $python (Join-Path $androidDir 'helpers/generate_store_assets.py') validate --output $OutputDirectory
if ($LASTEXITCODE) { throw 'Store asset integration validation failed' }
Write-Output 'PASS: launcher saves, default HUD, D1/D2 replay results, screenshot cadence, visible frame pacing and complete 30-second video'
