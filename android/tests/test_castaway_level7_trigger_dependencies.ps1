#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_castaway_level7_trigger_dependencies/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

$metadata = Get-Content (Join-Path $repoRoot 'game_data/mission_files/castaway_redux.json') -Raw | ConvertFrom-Json
$plannedLevel = @($metadata.levels | Where-Object level_num -eq 7)
if ($plannedLevel.Count -ne 1 -or $plannedLevel[0].route_status -ne 'ok') {
    throw 'Castaway level 7 metadata must retain its completed route when optional analysis exhausts its budget'
}

& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless `
    -MissionJson castaway_redux.json -Level 7 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Castaway level 7 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'castaway_redux.json_0_7_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = @(
    'blue key',
    'Shoot switch trigger 8',
    'Fly-through trigger 1',
    'gold key',
    'Fly-through trigger 13',
    'Shoot switch trigger 3',
    'Fly-through trigger 4',
    'Shoot switch trigger 5',
    'Fly-through trigger 41',
    'red key',
    'Reactor',
    'Exit'
)
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Castaway level 7 did not reach the exit: $($result.status), $($result.problem)"
    }
    if (($result.objectives.label -join '|') -cne ($expected -join '|')) {
        throw 'Castaway level 7 did not activate the closed-source prerequisites and complete its route in order'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Castaway level 7 simulation is not deterministic' }
}
$normalized = Get-Content (Join-Path $output 'results/castaway_redux.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 7)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Castaway level 7 physical objectives disagree with its metadata route'
}
Write-Host "PASS Castaway level 7 trigger dependencies: $($result.frames) frames, deterministic, metadata agrees"
