#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_castaway_level1_cage/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

# Keep corpus files untouched and compare two real engine runs
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless `
    -MissionJson castaway_redux.json -Level 1 -Repeat 2 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Castaway level 1 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'castaway_redux.json_0_1_*_run_*.json' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Castaway level 1 did not reach the exit: $($result.status), $($result.problem)"
    }
    $labels = @($result.objectives.label)
    $last = -1
    foreach ($required in @('blue key', 'gold key', 'red key', 'Fly-through trigger 20',
            'Shoot switch trigger 2', 'Reactor', 'Exit')) {
        $index = [Array]::IndexOf($labels, $required)
        if ($index -le $last) { throw "Missing/out-of-order objective: $required" }
        $last = $index
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch 'incidental trigger=1 seg=203 side=5 actor_seg=202') {
        throw 'Missing physical crossing of the cage-opening trigger'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -ne $first) { throw 'Castaway level 1 simulation is not deterministic' }
}
# Physical completion is distinct from static/live objective agreement
$normalized = Get-Content (Join-Path $output 'results/castaway_redux.simulation.json') -Raw | ConvertFrom-Json
if ($normalized.levels[0].status -notin @('ok', 'route_mismatch')) {
    throw "Unexpected normalized status: $($normalized.levels[0].status)"
}
Write-Host "PASS Castaway level 1 cage: $($result.frames) frames, deterministic; metadata=$($normalized.levels[0].status)"
