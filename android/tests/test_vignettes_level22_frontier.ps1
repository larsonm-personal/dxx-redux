#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_vignettes_level22_frontier/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
# Match the development mission by its level file, excluding downloaded copies
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 22 -LevelFileFilter level44.rdl -Repeat 2 `
    -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Vignettes frontier simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeated physical runs' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne 'blue key|red key|Reactor|Exit') {
        throw 'Vignettes L22 must complete both keys, reactor and exit'
    }
    if ($result.radius.player -ne 310325 -or $result.radius.effective -ne $result.radius.player) {
        throw 'Frontier verification must retain the full player radius'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'physical_target semantic=73 navigation=133 player_key_flags=2 frontier_keyed=1' -or
        $log -notmatch 'flare wall=79 seg=133 side=5 actor_seg=132') {
        throw 'Expected guidance to the useful blue-key door and its actual opening shot'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Vignettes frontier repeats differ'
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/Vignettes.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 22)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') { throw 'Physical completion disagrees with the metadata route' }
Write-Host "PASS Vignettes L22: useful keyed-door frontiers and repeated complete route, $($result.frames) frames"
