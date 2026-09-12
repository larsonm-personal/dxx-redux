#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_castaway_level5_switch_dependencies/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

$metadata = Get-Content (Join-Path $repoRoot 'game_data/mission_files/castaway_redux.json') -Raw | ConvertFrom-Json
$plannedLevel = @($metadata.levels | Where-Object level_num -eq 5)
if ($plannedLevel.Count -ne 1 -or $plannedLevel[0].route_status -ne 'ok') {
    throw 'Castaway level 5 metadata must include a complete route through the switch prerequisites'
}

& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless `
    -MissionJson castaway_redux.json -Level 5 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Castaway level 5 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'castaway_redux.json_0_5_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = @(
    'blue key',
    'gold key',
    'Fly-through trigger 2',
    'Shoot switch trigger 1',
    'Shoot switch trigger 12',
    'Fly-through trigger 14',
    'Shoot switch trigger 13',
    'Fly-through trigger 17',
    'Fly-through trigger 19',
    'Shoot switch trigger 16',
    'Fly-through trigger 7',
    'red key',
    'Reactor',
    'Exit'
)
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Castaway level 5 did not reach the exit: $($result.status), $($result.problem)"
    }
    if (($result.objectives.label -join '|') -cne ($expected -join '|')) {
        throw 'Castaway level 5 did not activate the closed-source prerequisites and complete its route in order'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Castaway level 5 simulation is not deterministic' }
}
$normalized = Get-Content (Join-Path $output 'results/castaway_redux.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 5)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Castaway level 5 physical objectives disagree with its metadata route'
}
Write-Host "PASS Castaway level 5 trigger dependencies: $($result.frames) frames, deterministic, metadata agrees"
