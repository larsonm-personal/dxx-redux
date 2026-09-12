#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_ffyl_fleeing_guidebot/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 23 -LevelFileFilter PDL28.RL2 -Repeat 2 `
    -NoBuild:$NoBuild -MaxParallel 1 -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'FFYL simulation infrastructure failed' }
$aggregate = Get-Content -LiteralPath (Join-Path $output 'results/FFYL.simulation.json') -Raw | ConvertFrom-Json
$level = @($aggregate.levels | Where-Object level_num -EQ 23)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'The physical exit route must also agree with the saved metadata'
}
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two FFYL level 23 simulations' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or $result.radius.player -ne 310325 -or
        $result.radius.effective -ne $result.radius.player -or
        $result.objectives[-1].activation_kind -ne 'enter_exit') {
        throw 'Exit prerequisites must lead to full-radius mine completion'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch '(?s)complete step=.*label=Fly-through trigger 3.*complete step=.*label=blue key.*complete step=.*label=red key.*complete step=.*label=Boss robot.*complete step=.*label=Exit' -or
        $log -match 'guided missile') {
        throw 'Explicit route guidance must return through the player-start branch and complete the mine'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'FFYL physical repeats differ'
}
Write-Host 'PASS repeated full-radius FFYL level 23 completion despite the Guide-Bot run-away behavior'
