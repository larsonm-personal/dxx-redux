#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_tew_tapered_route/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 9 -LevelFileFilter 'level09.rl2' -Repeat 2 `
    -NoBuild:$NoBuild -MaxParallel 1 -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'TEW simulation infrastructure failed' }
$aggregate = Get-Content -LiteralPath (Join-Path $output 'results/TEW.simulation.json') -Raw | ConvertFrom-Json
$level = @($aggregate.levels | Where-Object level_num -EQ 9)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'The physical exit route must agree with the saved metadata'
}
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeats of TEW L9' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or $result.radius.player -ne 308279 -or
        $result.radius.effective -ne $result.radius.player -or
        $result.objectives[-1].activation_kind -ne 'enter_exit' -or
        @($result.objectives | Where-Object label -EQ 'gold key').Count -ne 1 -or
        @($result.objectives | Where-Object activation_kind -EQ 'destroy_reactor').Count -ne 1) {
        throw 'Tapered passage recovery must lead to full-radius mine completion'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'TEW L9 physical repeats differ'
}
Write-Host 'PASS repeated full-radius TEW L9 tapered passage regression'
