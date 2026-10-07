#!/usr/bin/env pwsh
# Capture and measure Android music/effects together using the production mixer
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$Output = 'temp/audio-mix-normalization',
    [string]$Python = 'temp/music-spectral-venv/Scripts/python.exe'
)

$ErrorActionPreference = 'Stop'
if (-not $Serial) { $Serial = 'emulator-5554' }
$repoRoot = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
Push-Location $repoRoot
try {
    & $Python android/tests/measure_audio_mix.py --serial $Serial --output $Output
    if ($LASTEXITCODE -ne 0) { throw 'Gameplay audio normalization checks failed' }
} finally {
    Pop-Location
}
