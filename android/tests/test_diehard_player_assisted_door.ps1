#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_diehard_player_assisted_door/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 9, 19 -LevelFileFilter d2levc-1.rl2, DH19.RL2 -Repeat 2 `
    -NoBuild:$NoBuild -MaxParallel 1 -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Die Hard simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 4) { throw 'Expected two physical runs each for Die Hard 19 and Counterstrike 9' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or
        $result.radius.player -ne 310325 -or $result.radius.effective -ne $result.radius.player) {
        throw 'Player assistance must complete the mine at full player radius'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($result.level -eq 19 -and
        (($result.objectives.activation_kind -join '|') -ne 'destroy_boss|enter_exit' -or
        $log -notmatch 'flare wall=5 seg=3 side=0 actor_seg=3' -or $log -match 'close flare fallback|guided missile')) {
        throw 'The Buddy-proof door must open through an ordinary player projectile'
    }
    if ($result.level -eq 9 -and
        ($log -notmatch 'flare wall=85 seg=512 side=4 actor_seg=512' -or
        $result.objectives[-1].activation_kind -ne 'enter_exit')) {
        throw 'Player assistance must preserve the ordinary hidden-door approach in Counterstrike 9'
    }
}
foreach ($pair in $files | Group-Object { $_.Name -replace '_run_[0-9]+\.json$', '' }) {
    if ($pair.Count -ne 2 -or (Get-FileHash $pair.Group[0].FullName).Hash -ne (Get-FileHash $pair.Group[1].FullName).Hash) {
        throw 'Physical repeats differ'
    }
}
Write-Host 'PASS repeated full-radius completions: Die Hard 19 Buddy-proof door and Counterstrike 9 ordinary hidden door'
