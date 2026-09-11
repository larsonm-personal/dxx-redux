#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_diehard_exit_prerequisite/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 6 -LevelFileFilter DH06.RL2 -Repeat 2 `
    -NoBuild:$NoBuild -MaxParallel 1 -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Die Hard simulation infrastructure failed' }
$aggregate = Get-Content -LiteralPath (Join-Path $output 'results/diehard.simulation.json') -Raw | ConvertFrom-Json
$level = @($aggregate.levels | Where-Object level_num -EQ 6)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'The physical exit route must also agree with the saved metadata'
}
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two Die Hard level 6 simulations' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or $result.radius.player -ne 310325 -or
        $result.radius.effective -ne $result.radius.player -or
        $result.objectives[-1].activation_kind -ne 'enter_exit') {
        throw 'Exit prerequisites must lead to full-radius mine completion'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch '(?s)complete step=.*label=Boss robot.*complete step=.*label=Open hidden door.*complete step=.*label=Shoot switch trigger 9.*complete step=.*label=Exit' -or
        $log -notmatch 'verified switch shot actor_seg=566 target_seg=568 wall=70' -or
        $log -match 'guided missile') {
        throw 'The route must open the solid exit barrier through its actual switch'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Die Hard physical repeats differ'
}
Write-Host 'PASS repeated full-radius Die Hard level 6 completion after opening its exit barrier'
