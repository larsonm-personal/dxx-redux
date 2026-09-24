#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Hog,
    [ValidateRange(10, 119)][int]$Seconds = 60,
    [string]$Work = 'temp/fm-quality',
    [string]$Output
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../../helpers/test_env.ps1"
$repo = (Resolve-Path "$PSScriptRoot/../../..").Path
Push-Location $repo
try {
    $hogPath = (Resolve-Path -LiteralPath $Hog).Path
    if (-not $Output) { $Output = Join-Path $Work 'listening' }
    & "$PSScriptRoot/../../helpers/retain-recent-artifacts.ps1" -Artifacts @($Work, $Output)
    New-Item -ItemType Directory -Path $Work -Force | Out-Null
    $workPath = (Resolve-Path -LiteralPath $Work).Path
    $venv = Join-Path $workPath 'venv'
    $python = Join-Path $venv 'Scripts/python.exe'
    if (-not (Test-Path -LiteralPath $python)) {
        python -m venv $venv
        if ($LASTEXITCODE -ne 0) { throw 'Python environment creation failed' }
    }
    & $python -c 'import numpy, scipy; assert numpy.__version__ == "2.2.6" and scipy.__version__ == "1.15.3"' 2>$null
    if ($LASTEXITCODE -ne 0) {
        & $python -m pip install numpy==2.2.6 scipy==1.15.3
        if ($LASTEXITCODE -ne 0) { throw 'Analysis dependency installation failed' }
    }
    $build = Join-Path $workPath 'build'
    cmake -S $PSScriptRoot -B $build -G 'Visual Studio 17 2022' -A x64 "-DPython3_EXECUTABLE=$python"
    if ($LASTEXITCODE -ne 0) { throw 'Experiment configure failed' }
    cmake --build $build --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Experiment build failed' }
    ctest --test-dir $build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Streaming FM regression checks failed' }
    & $python "$PSScriptRoot/experiment.py" --build "$build/Release" --hog $hogPath --output $Output --seconds $Seconds
    if ($LASTEXITCODE -ne 0) { throw 'FM quality experiment failed' }
} finally {
    Pop-Location
}
