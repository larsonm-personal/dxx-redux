#!/usr/bin/env pwsh
param(
    [string]$Exe,
    [string]$DataDir,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $Exe) {
    $Exe = Join-Path $repoRoot 'buildd2\main\dxx-redux-d2-headless-metadata.exe'
}
if (-not $DataDir) {
    $DataDir = Join-Path $repoRoot 'game_data_to_copy_to_emulator\temp'
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot 'android\temp\counterstrike_level2_trigger21_route.json'
}

if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
    throw "D2 headless metadata executable not found: $Exe"
}
if (-not (Test-Path -LiteralPath $DataDir -PathType Container)) {
    throw "D2 game data directory not found: $DataDir"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
& $Exe -hogdir $DataDir -secretarea-json-out $OutputPath | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Counterstrike metadata dump failed with exit code $LASTEXITCODE"
}

$metadata = Get-Content -LiteralPath $OutputPath -Raw | ConvertFrom-Json
$level = @($metadata.levels | Where-Object { $_.level_num -eq 2 })
if ($level.Count -ne 1) {
    throw "Expected exactly one Counterstrike level 2 record, found $($level.Count)"
}
$step = @($level[0].route_steps | Where-Object { $_.trigger -eq 21 })
if ($step.Count -ne 1) {
    throw "Expected exactly one trigger 21 route step, found $($step.Count)"
}
if ($step[0].activation_kind -ne 'shoot_switch') {
    throw "Trigger 21 route step is $($step[0].activation_kind), expected shoot_switch"
}
if ($step[0].calculated -eq $false) {
    throw 'Trigger 21 route step is still marked not calculated'
}
if ($step[0].seg -eq $step[0].opens[0].seg) {
    throw 'Trigger 21 still routes to its opened wall instead of a firing waypoint'
}

Write-Host "PASS Counterstrike level 2 trigger 21 firing waypoint: segment $($step[0].seg)"
