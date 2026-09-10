#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_lostlvls_level22_portal/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 22 -LevelFileFilter lost22.rl2 -Repeat 2 `
    -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Lost Levels portal simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeated physical runs' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne
        'Destroy robot carrying blue key|Destroy robot carrying gold key|Destroy robot carrying red key|Reactor|Exit') {
        throw 'Lost Levels L22 must complete all three key carriers, reactor and exit'
    }
    if ($result.radius.player -ne 426296 -or $result.radius.effective -ne $result.radius.player) {
        throw 'Portal verification must retain the full player radius'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch '(?s)recover portal actor_seg=347 approach_seg=347.*complete step=2 .*label=Destroy robot carrying gold key') {
        throw 'Expected recovery through the stalled passage before collecting the gold key'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Lost Levels portal repeats differ'
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/Lostlvls.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 22)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') { throw 'Physical completion disagrees with the metadata route' }
Write-Host "PASS Lost Levels L22: full-radius portal recovery and repeated complete route, $($result.frames) frames"
