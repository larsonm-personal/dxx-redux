#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_lostlvls_door_recess/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 9, 14, 15 -LevelFileFilter d2levc-1.rl2, lost14.rl2, lost15.rl2 -Repeat 2 `
    -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Lost Levels door recess simulation infrastructure failed' }
$allFiles = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($allFiles.Count -ne 6) { throw 'Expected two runs of each of the three door fixtures' }
foreach ($case in @('Counterstrike.json_0_9_d2levc-1.rl2', 'Lostlvls.json_0_15_lost15.rl2')) {
    $runs = @($allFiles | Where-Object { $_.Name.StartsWith($case + '_run_') })
    if ($runs.Count -ne 2) { throw "Expected two runs of $case" }
    foreach ($file in $runs) {
        $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        if ($result.status -ne 'confirmed') { throw "$case ordinary door traversal regressed" }
    }
    if ((Get-FileHash $runs[0].FullName).Hash -ne (Get-FileHash $runs[1].FullName).Hash) {
        throw "$case repeats differ"
    }
}
$files = @($allFiles | Where-Object Name -Like 'Lostlvls.json_0_14_*')
if ($files.Count -ne 2) { throw 'Expected two repeated physical runs' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Shoot switch trigger 16|blue key|Shoot switch trigger 10|Fly-through trigger 11|Shoot switch trigger 12|Open hidden door|Shoot switch trigger 0|Open hidden door|Shoot switch trigger 1|gold key|Shoot switch trigger 1|Open hidden door|Shoot switch trigger 15|red key|Reactor|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Lost Levels L14 must complete its actual switch sequence, keys, reactor and exit'
    }
    if ($result.radius.player -ne 426296 -or $result.radius.effective -ne $result.radius.player) {
        throw 'Door recess verification must retain the full player radius'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch '(?s)flare wall=20 seg=54 side=3.*complete step=8.*verified switch shot actor_seg=56 target_seg=54 wall=23.*complete step=9') {
        throw 'The door must open through a real flare and expose the switch shot from outside the recess'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Lost Levels door recess repeats differ'
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/Lostlvls.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 14)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Metadata disagrees with physical completion' }
Write-Host "PASS Lost Levels L14: native door opening, outside switch shot and repeated full route, $($result.frames) frames"
