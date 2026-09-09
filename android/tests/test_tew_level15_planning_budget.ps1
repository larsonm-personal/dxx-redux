#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_tew_level15_planning_budget/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson TEW.json -Level 15 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'TEW level 15 simulation runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'TEW.json_0_15_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = 'blue key|Shoot switch trigger 2|Pass through trigger 6|Shoot switch trigger 16|Destroy blastable wall|Open hidden door|Boss robot|Exit'
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $expected) {
        throw "TEW level 15 did not complete its key and boss sequence: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'TEW level 15 simulation is not deterministic' }
}
$metadata = Get-Content (Join-Path $repoRoot 'game_data/mission_files/TEW.json') -Raw | ConvertFrom-Json
$planned = @($metadata[0].levels | Where-Object level_num -eq 15)[0]
if ($planned.route_status -ne 'ok' -or $planned.route_note -notlike '*key-route optimization reached its work budget*') {
    throw 'TEW level 15 must retain its complete route after key optimization exhausts the budget'
}
$normalized = Get-Content (Join-Path $output 'results/TEW.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 15)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'TEW level 15 physical objectives disagree with metadata'
}
Write-Host "PASS TEW level 15 planning budget: $($result.frames) frames, deterministic, metadata agrees"
