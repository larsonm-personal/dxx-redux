#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_tew_level23_hidden_keyed_door/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson TEW.json -Level 23 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'TEW level 23 simulation runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'TEW.json_0_23_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = 'blue key|red key|Reactor|Exit'
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $expected) {
        throw "TEW level 23 did not complete its key and reactor sequence: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'TEW level 23 simulation is not deterministic' }
    $logName = $file.BaseName + '.log'
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$logName") -Raw
    if (-not $log.Contains('flare wall=0 seg=77 side=0 actor_seg=77')) {
        throw 'The keyed door must receive a physical flare after collecting the blue key'
    }
}
$normalized = Get-Content (Join-Path $output 'results/TEW.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 23)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'TEW level 23 physical objectives disagree with metadata'
}
Write-Host "PASS TEW level 23 hidden keyed door: $($result.frames) frames, deterministic, metadata agrees"
