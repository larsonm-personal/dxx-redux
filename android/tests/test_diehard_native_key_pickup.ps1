#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_diehard_native_key_pickup/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 8 -LevelFileFilter DH08.RL2 -Repeat 2 `
    -NoBuild:$NoBuild -MaxParallel 1 -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Native key pickup simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two physical runs' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -ne 'red key|Boss robot|Exit' -or
        $result.radius.player -ne 310325 -or $result.radius.effective -ne $result.radius.player) {
        throw 'Native pickup must allow the complete key, boss and exit route at full player radius'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'RED Access granted' -or
        $log -notmatch 'complete step=1 frame=1 .*label=red key' -or
        $log -notmatch 'semantic=0 navigation=0 player_key_flags=0' -or
        $log -notmatch 'semantic=110 navigation=24 player_key_flags=4') {
        throw 'The starting key must be earned through native pickup before continuing'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) { throw 'Physical repeats differ' }
Write-Host "PASS Die Hard L8: native starting-key pickup and repeated full mine completion, $($result.frames) frames"
