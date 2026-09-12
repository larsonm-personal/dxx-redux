#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_tew_level1_adjacent_switch/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson TEW.json -Level 1 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'TEW level 1 simulation runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'TEW.json_0_1_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = 'blue key|Shoot switch trigger 2|gold key|Shoot switch trigger 3|red key|Reactor|Exit'
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $expected) {
        throw "TEW level 1 did not complete its switch and key sequence: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'TEW level 1 simulation is not deterministic' }
    $logName = $file.BaseName + '.log'
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$logName") -Raw
    if ($log -notmatch 'verified switch shot actor_seg=[0-9]+ target_seg=86 wall=34') {
        throw 'The first switch must use a verified shot before arriving at its waypoint'
    }
}
$normalized = Get-Content (Join-Path $output 'results/TEW.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 1)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'TEW level 1 physical objectives disagree with metadata'
}
Write-Host "PASS TEW level 1 adjacent switch: $($result.frames) frames, deterministic, metadata agrees"
