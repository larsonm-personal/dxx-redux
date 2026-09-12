#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_vignettes_level8_firing_position/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson Vignettes.json -Level 8 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Vignettes level 8 simulation runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Vignettes.json_0_8_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne 'blue key|red key|Reactor|Exit') {
        throw "Vignettes level 8 did not complete its reactor firing position and exit: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Vignettes level 8 simulation is not deterministic' }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch '(?s)goal step=3.*goal_seg=201 step_seg=272 path_terminal=201.*complete step=3.*label=Reactor.*complete step=4.*label=Exit') {
        throw 'The simulation must complete the reactor from its remote firing position and then exit'
    }
}
$normalized = Get-Content (Join-Path $output 'results/Vignettes.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 8)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Vignettes level 8 physical objectives disagree with metadata'
}
Write-Host "PASS Vignettes level 8 firing position: $($result.frames) frames, deterministic, metadata agrees"
