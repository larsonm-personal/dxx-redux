#!/usr/bin/env pwsh
# Capture and measure Android music/effects together using the production mixer
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$Output = 'temp/audio-mix-normalization',
    [string]$Python
)

$ErrorActionPreference = 'Stop'
if (-not $Serial) { $Serial = 'emulator-5554' }
$repoRoot = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
Push-Location $repoRoot
try {
    if (-not $Python) {
        $Python = if ([Environment]::OSVersion.Platform -eq 'Win32NT') {
            'temp/music-spectral-venv/Scripts/python.exe'
        } else {
            'temp/music-spectral-venv/bin/python'
        }
    }
    . ./android/helpers/jsonc.ps1
    $Output = [IO.Path]::GetFullPath($Output)
    New-Item -ItemType Directory -Force $Output | Out-Null
    $template = ConvertFrom-JsoncText -Text (Get-Content android/game_scripts/test_audio_mix_unified.jsonc -Raw)
    $templatePath = Join-Path $Output 'template.json'
    $template | Set-Content $templatePath -Encoding utf8
    & $Python android/tests/measure_audio_mix.py --serial $Serial --output $Output --template $templatePath
    if ($LASTEXITCODE -ne 0) { throw 'Gameplay audio normalization checks failed' }
} finally {
    Pop-Location
}
