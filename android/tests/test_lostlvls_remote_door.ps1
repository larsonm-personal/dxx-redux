#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_lostlvls_remote_door/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionJson Lostlvls.json -Level 9 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Lost Levels remote-door simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeated physical runs' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne
        'Open hidden door|blue key|Destroy robot carrying gold key|red key|Reactor|Exit') {
        throw 'Lost Levels L9 must open the remote door and complete keys, reactor and exit'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'flare wall=44 seg=198 side=2 actor_seg=185' -or
        $log -match 'close flare fallback wall=44' -or $log -match 'guided missile') {
        throw 'The remote door must open through ordinary projectile flight, without the close-door fallback'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Lost Levels repeated physical runs differ'
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/Lostlvls.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 9)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') { throw 'Lost Levels physical completion disagrees with the metadata route' }
Write-Host "PASS Lost Levels L9: repeated ordinary remote-door flight and complete route, $($result.frames) frames"
