#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_castaway_level8_keyed_trigger_route/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

$metadata = Get-Content (Join-Path $repoRoot 'game_data/mission_files/castaway_redux.json') -Raw | ConvertFrom-Json
$plannedLevel = @($metadata.levels | Where-Object level_num -eq 8)
if ($plannedLevel.Count -ne 1 -or $plannedLevel[0].route_status -ne 'ok') {
    throw 'Castaway level 8 metadata must include a complete route through the switch prerequisites'
}

& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless `
    -MissionJson castaway_redux.json -Level 8 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Castaway level 8 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'castaway_redux.json_0_8_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = @(
    'blue key',
    'Pass through trigger 3',
    'Shoot switch trigger 4',
    'gold key',
    'Fly-through trigger 15',
    'Shoot switch trigger 24',
    'Fly-through trigger 14',
    'Shoot switch trigger 25',
    'Fly-through trigger 16',
    'Shoot switch trigger 26',
    'Fly-through trigger 43',
    'Shoot switch trigger 17',
    'Fly-through trigger 18',
    'red key',
    'Pass through trigger 9',
    'Fly-through trigger 49',
    'Reactor',
    'Exit'
)
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Castaway level 8 did not reach the exit: $($result.status), $($result.problem)"
    }
    if (($result.objectives.label -join '|') -cne ($expected -join '|')) {
        throw 'Castaway level 8 did not activate the keyed-door and switch prerequisites and complete its route in order'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Castaway level 8 simulation is not deterministic' }
}
$normalized = Get-Content (Join-Path $output 'results/castaway_redux.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 8)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Castaway level 8 physical objectives disagree with its metadata route'
}
Write-Host "PASS Castaway level 8 keyed trigger route: $($result.frames) frames, deterministic, metadata agrees"
