#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_diehard_opening_frontier/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -LevelFileFilter @('DH07.RL2', 'alarlof.rdl') -Repeat 2 `
    -NoBuild:$NoBuild -MaxParallel 1 -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Die Hard simulation infrastructure failed' }
$aggregate = Get-Content -LiteralPath (Join-Path $output 'results/diehard.simulation.json') -Raw | ConvertFrom-Json
$level = @($aggregate.levels | Where-Object level_num -EQ 7)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'The physical exit route must also agree with the saved metadata'
}
$af = Get-Content -LiteralPath (Join-Path $output 'results/af_d1_beta.simulation.json') -Raw | ConvertFrom-Json
$afLevel = @($af.levels | Where-Object level_num -EQ 6)
if ($afLevel.Count -ne 1 -or $afLevel[0].status -ne 'ok') {
    throw 'The existing AF D1 L6 route must remain passing'
}
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 4) { throw 'Expected two repeats each of Die Hard L7 and AF D1 L6' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or $result.radius.player -ne 310325 -or
        $result.radius.effective -ne $result.radius.player -or
        $result.objectives[-1].activation_kind -ne 'enter_exit') {
        throw 'Door animation must lead to full-radius mine completion'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($file.Name.StartsWith('diehard.') -and
        ($log -notmatch '(?s)flare wall=48 seg=339 side=5 actor_seg=339.*complete step=.*label=Shoot switch trigger 3.*complete step=.*label=red key.*complete step=.*label=Exit' -or
        $log -match 'close flare fallback wall=48\b|guided missile')) {
        throw 'The route must cross the opened blue door and finish through native actions'
    }
}
foreach ($pair in ($files | Group-Object { $_.Name -replace '_run_[0-9]+\.json$', '' })) {
    if ($pair.Count -ne 2 -or
        (Get-FileHash $pair.Group[0].FullName).Hash -ne (Get-FileHash $pair.Group[1].FullName).Hash) {
        throw 'Door animation physical repeats differ'
    }
}
Write-Host 'PASS repeated full-radius Die Hard L7 and AF D1 L6 door animation regressions'
