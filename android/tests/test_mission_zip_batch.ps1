#!/usr/bin/env pwsh

param(
    [int]$MaxZips = 3
)

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path -Parent $PSScriptRoot
$batchScript = Join-Path $androidRoot "helpers\run_mission_zip_batch.ps1"

if (-not (Test-Path -LiteralPath $batchScript)) {
    Write-Host "FAIL: mission ZIP batch script not found at $batchScript" -ForegroundColor Red
    exit 1
}

Write-Host "Running mission ZIP batch sample with MaxZips=$MaxZips"
& $batchScript -MaxZips $MaxZips
exit $LASTEXITCODE
