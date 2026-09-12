#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_castaway_level6_trigger/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless `
    -MissionJson castaway_redux.json -Level 6 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Castaway level 6 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'castaway_redux.json_0_6_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = @(
    'Shoot switch trigger 3', 'blue key', 'Fly-through trigger 1',
    'Shoot switch trigger 2', 'gold key', 'Shoot switch trigger 13', 'Fly-through trigger 18',
    'Shoot switch trigger 19', 'Fly-through trigger 38', 'red key', 'Fly-through trigger 9',
    'Shoot switch trigger 10', 'Fly-through trigger 12', 'Shoot switch trigger 11',
    'Shoot switch trigger 21', 'Boss robot', 'Exit'
)
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Castaway level 6 did not reach the exit: $($result.status), $($result.problem)"
    }
    if (($result.objectives.label -join '|') -cne ($expected -join '|')) {
        throw 'Castaway level 6 did not complete its switch restoration and key dependencies in order'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Castaway level 6 simulation is not deterministic' }
}
$normalized = Get-Content (Join-Path $output 'results/castaway_redux.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 6)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Castaway level 6 physical objectives disagree with its metadata route'
}
Write-Host "PASS Castaway level 6 trigger route: $($result.frames) frames, deterministic, metadata agrees"
