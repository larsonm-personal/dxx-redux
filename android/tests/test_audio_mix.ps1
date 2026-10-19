#!/usr/bin/env pwsh
# Capture and measure Android music/effects together using the production mixer
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$Output = 'temp/audio-mix-normalization',
    [string]$Python
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$repoRoot = Split-Path $PSScriptRoot -Parent | Split-Path -Parent
Push-Location $repoRoot
try {
    if (-not $Python) {
        $environmentPath = Join-Path $repoRoot 'temp/music-spectral-venv'
        $Python = if (Test-RegressionWindowsHost) {
            Join-Path $environmentPath 'Scripts/python.exe'
        } else {
            Join-Path $environmentPath 'bin/python'
        }
        if (-not (Test-Path -LiteralPath $Python)) {
            $bootstrapPython = Resolve-RegressionPythonCommand
            if (-not $bootstrapPython) { throw 'Python 3 is required for audio measurements' }
            $prefixArguments = @($bootstrapPython.PrefixArguments)
            & $bootstrapPython.Path @prefixArguments -m venv $environmentPath
            if ($LASTEXITCODE -ne 0) { throw 'Could not create the audio measurement environment' }
        }
        & $Python -c 'import numpy, scipy, matplotlib, imageio_ffmpeg' 2>$null
        if ($LASTEXITCODE -ne 0) {
            & $Python -m pip install --disable-pip-version-check -r (Join-Path $PSScriptRoot 'music_spectral/requirements.txt')
            if ($LASTEXITCODE -ne 0) { throw 'Could not install pinned audio measurement dependencies' }
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
