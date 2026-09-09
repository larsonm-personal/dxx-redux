#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_tew_secret3_trigger_door/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson TEW.json -Level -3 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'TEW secret -3 simulation runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'TEW.json_0_-3_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$metadata = Get-Content (Join-Path $repoRoot 'game_data/mission_files/TEW.json') -Raw | ConvertFrom-Json
$planned = @($metadata[0].levels | Where-Object level_num -eq -3)[0]
$expected = @($planned.route_steps | Where-Object kind -ne 'start').label -join '|'
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $expected) {
        throw "TEW secret -3 did not complete its planned objectives: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'TEW secret -3 simulation is not deterministic' }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch '(?s)flare wall=211 seg=763 side=0.*complete step=9.*label=Pass through trigger 5.*goal step=10.*actor_seg=765') {
        throw 'The trigger door must be shot open and physically crossed before the next objective'
    }
}
$normalized = Get-Content (Join-Path $output 'results/TEW.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq -3)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'TEW secret -3 physical objectives disagree with metadata'
}
Write-Host "PASS TEW secret -3 trigger door: $($result.frames) frames, deterministic, metadata agrees"
