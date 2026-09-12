#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_ffyl_forcefield_door/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 5 -LevelFileFilter PDL12.RL2 -Repeat 2 `
    -NoBuild:$NoBuild -MaxParallel 1 -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'FFYL simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two FFYL level 5 simulations' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or $result.radius.player -ne 310325 -or
        $result.radius.effective -ne $result.radius.player -or
        $result.objectives[-1].activation_kind -ne 'enter_exit') {
        throw 'Force-field door assistance must complete the mine at full player radius'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'flare wall=18 seg=47 side=2 actor_seg=47 weapon=11' -or
        $log -match 'close flare fallback wall=18\b|guided missile') {
        throw 'The force-field door must open through a native ballistic projectile'
    }
    if ($log -notmatch 'BLUE Access granted' -or $log -notmatch 'YELLOW Access granted' -or
        $log -notmatch 'RED Access granted') {
        throw 'The route must collect its keys through native contact'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'FFYL physical repeats differ'
}
Write-Host 'PASS repeated full-radius FFYL level 5 completion through a force-field-textured door'
