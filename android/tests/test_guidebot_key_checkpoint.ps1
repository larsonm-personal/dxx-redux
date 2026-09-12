#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild, [string]$OutputRoot)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = if ($OutputRoot) { [IO.Path]::GetFullPath($OutputRoot) } else { Join-Path $repoRoot ('android/temp/test_guidebot_key_checkpoint/' + (Get-Date -Format 'yyyyMMdd_HHmmss')) }
# Reuse the regression staging path for this exact mission and archive variant
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -RoutingDevelopmentSet -Level 4 -LevelFileFilter lost04.rl2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Key checkpoint fixture staging failed' }
$stages = @(Get-ChildItem -LiteralPath (Join-Path $output 'stages') -Directory)
if ($stages.Count -ne 1) { throw 'Expected one staged mission' }
$exe = Join-Path $repoRoot 'buildd2/main/dxx-redux-d2-headless-route.exe'
$hog = Join-Path $repoRoot 'game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data'
foreach ($run in 1..2) {
    $userDir = Join-Path $output "key-user$run"
    New-Item -ItemType Directory -Path $userDir | Out-Null
    $common = @('-hogdir', $hog, '-extra-dir', $stages[0].FullName, '-mission', 'LOSTLVLS', '-level', '4', '-route-confirm-user-dir', $userDir)
    $resultPath = Join-Path $output "key$run.json"
    & $exe @common -route-confirm-key blue -route-confirm-checkpoint-out blue.sg0 `
        -route-confirm-json-out $resultPath *> (Join-Path $output "key$run.log")
    if ($LASTEXITCODE -ne 0) { throw 'Native blue key pickup/checkpoint failed' }
    $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or $result.frames -eq 0 -or
        $result.verification_goal.key -ne 'blue' -or $result.verification_goal.initial_key_flags -ne 0 -or
        $result.objectives[-1].label -ne 'blue key' -or $null -ne $result.route_confirmation -or
        $result.radius.player -ne $result.radius.effective) {
        throw 'Optional blue key must be physically acquired at full radius, without a mine-completion certificate'
    }
    $save = Join-Path $userDir 'blue.sg0'
    $hash = (Get-FileHash -LiteralPath $save).Hash
    $restoredPath = Join-Path $output "restored$run.json"
    & $exe @common -route-confirm-key blue -route-confirm-checkpoint blue.sg0 `
        -route-confirm-json-out $restoredPath *> (Join-Path $output "restored$run.log")
    if ($LASTEXITCODE -ne 0) { throw 'Native key checkpoint restore failed' }
    $restored = Get-Content -LiteralPath $restoredPath -Raw | ConvertFrom-Json
    if ($restored.status -ne 'confirmed' -or $restored.frames -ne 0 -or
        $restored.start_state.key_flags -ne 2 -or $restored.start_state.segment -ne 83 -or
        (Get-FileHash -LiteralPath $save).Hash -ne $hash) {
        throw 'Restored native checkpoint lost the earned key, pickup position or input integrity'
    }
}
if ((Get-FileHash (Join-Path $output 'key1.json')).Hash -ne (Get-FileHash (Join-Path $output 'key2.json')).Hash -or
    (Get-FileHash (Join-Path $output 'restored1.json')).Hash -ne (Get-FileHash (Join-Path $output 'restored2.json')).Hash) {
    throw 'Key pickup or checkpoint restoration repeats differ'
}
$missingPath = Join-Path $output 'missing.json'
& $exe @common -route-confirm-key gold -route-confirm-checkpoint-out missing.sg0 `
    -route-confirm-json-out $missingPath *> (Join-Path $output 'missing.log')
if ($LASTEXITCODE -ne 2 -or (Test-Path -LiteralPath (Join-Path $userDir 'missing.sg0'))) { throw 'Missing gold key was invented or checkpointed' }
$missing = Get-Content -LiteralPath $missingPath -Raw | ConvertFrom-Json
if ($missing.status -ne 'failed' -or $missing.frames -ne 0 -or $missing.verification_goal.initial_key_flags -ne 0) {
    throw 'Missing key outcome is incorrect'
}
Write-Host "PASS Lost Levels L4: native optional blue pickup and checkpoint restoration, $($result.frames) frames"
exit 0
