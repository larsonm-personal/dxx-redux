#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_tew_level26_timed_switches/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson TEW.json -Level 26 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'TEW level 26 simulation runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'TEW.json_0_26_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = 'Shoot switch trigger 0|blue key|gold key|Fly-through trigger 11|Shoot switch trigger 10|Fly-through trigger 12|Shoot switch trigger 9|red key|Fly-through trigger 5|Boss robot|Exit'
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $expected) {
        throw "TEW level 26 did not complete its switch and key sequence: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'TEW level 26 simulation is not deterministic' }
    $logName = $file.BaseName + '.log'
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$logName") -Raw
    if (-not $log.Contains('verified switch shot actor_seg=161 target_seg=157 wall=34') -or
        -not $log.Contains('verified switch shot actor_seg=143 target_seg=155 wall=36')) {
        throw 'Both timed switches must be shot from their trigger corridors'
    }
    foreach ($indices in @(@(3, 4), @(5, 6))) {
        if ($result.objectives[$indices[1]].frame - $result.objectives[$indices[0]].frame -gt 60) {
            throw 'The timed switch shot must occur promptly after opening its door'
        }
    }
}
$normalized = Get-Content (Join-Path $output 'results/TEW.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 26)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'TEW level 26 physical objectives disagree with metadata'
}
Write-Host "PASS TEW level 26 timed switches: $($result.frames) frames, deterministic, metadata agrees"
