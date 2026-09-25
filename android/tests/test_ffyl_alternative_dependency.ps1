#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild, [string]$MissionMetadataRoot)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_ffyl_alternative_dependency/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -MissionMetadataRoot $MissionMetadataRoot -Level 4 -LevelFileFilter 'PDL11.RL2' -Repeat 2 `
    -NoBuild:$NoBuild -MaxParallel 1 -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'FFYL simulation infrastructure failed' }
$aggregate = Get-Content -LiteralPath (Join-Path $output 'results/FFYL.simulation.json') -Raw | ConvertFrom-Json
$level = @($aggregate.levels | Where-Object level_num -EQ 4)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'The physical exit route must agree with the saved metadata'
}
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeats of FFYL L4' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or $result.radius.player -ne 310325 -or
        $result.radius.effective -ne $result.radius.player -or
        $result.objectives[-1].activation_kind -ne 'enter_exit' -or
        ($result.objectives.label -join '|') -notmatch 'Shoot switch trigger 13.*Pass through trigger 12.*gold key.*red key.*Boss robot.*Exit') {
        throw 'The alternative switch dependency must lead to full-radius mine completion'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'FFYL L4 physical repeats differ'
}
Write-Host 'PASS repeated full-radius FFYL L4 alternative dependency regression'
