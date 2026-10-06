#!/usr/bin/env pwsh
# End-to-end asset integration: generate real media or validate a completed run
[CmdletBinding()]
param([string]$OutputDirectory, [switch]$FeaturedOnly)
$ErrorActionPreference = 'Stop'
if ($FeaturedOnly -and !$OutputDirectory) { throw '-FeaturedOnly requires an existing -OutputDirectory' }
$androidDir = Split-Path $PSScriptRoot
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $androidDir ('temp/store-assets_test_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
    & (Join-Path $androidDir 'generate-store-assets.ps1') -OutputDirectory $OutputDirectory
    if ($LASTEXITCODE) { throw 'Store asset generation failed' }
}
$python = Join-Path $androidDir $(if ($IsWindows) { 'temp/store-assets-tools/venv/Scripts/python.exe' } else { 'temp/store-assets-tools/venv/bin/python' })
if (!(Test-Path $python)) { $python = (Get-Command python -ErrorAction Stop).Source }
if (!$FeaturedOnly -and (Test-Path (Join-Path $OutputDirectory 'boss-edit.json'))) {
    & $python (Join-Path $androidDir 'helpers/store_asset_boss_video.py') validate --output $OutputDirectory
    if ($LASTEXITCODE) { throw 'Boss preview integration validation failed' }
    Write-Output 'PASS: reordered 30-second preview, muted launcher, original-tempo boss MIDI, preserved reviewed audio and smooth native fly-out'
    return
}
$action = if ($FeaturedOnly) { 'validate-featured' } elseif (Test-Path (Join-Path $OutputDirectory 'combined-edit.json')) { 'validate-combined' } else { 'validate' }
& $python (Join-Path $androidDir 'helpers/generate_store_assets.py') $action --output $OutputDirectory
if ($LASTEXITCODE) { throw 'Store asset integration validation failed' }
if ($FeaturedOnly) {
    Write-Output 'PASS: native 2048x1000 capture, ready-to-use 1024x500 RGB PNG with exact 2x Lanczos downsample, pinned frame, boss/progress rows, configured cameras and filtered graphics'
    return
}
if ($action -eq 'validate-combined') {
    Write-Output 'PASS: combined video is 30 seconds/900 frames, delivered audio exactly matches the reviewed reference, and native camera motion passes geometric checks'
    return
}
Write-Output 'PASS: launcher saves, default HUD, trilinear/4x MSAA/16x AF, primary D1/D2 replay results, screenshot cadence, frame pacing, engine audio, robot movies, 30-second video, eight selected images and native featured image (supplementary replay status is reported separately)'
