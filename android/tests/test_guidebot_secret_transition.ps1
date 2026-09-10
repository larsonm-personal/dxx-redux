#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_guidebot_secret_transition/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $output
$source = Join-Path $output 'source'
& (Join-Path $PSScriptRoot 'test_guidebot_key_checkpoint.ps1') -NoBuild:$NoBuild -OutputRoot $source
if ($LASTEXITCODE -ne 0) { throw 'Earned-key source scenario failed' }
$stages = @(Get-ChildItem -LiteralPath (Join-Path $source 'stages') -Directory)
if ($stages.Count -ne 1) { throw 'Expected one staged source mission' }
$exe = Join-Path $repoRoot 'buildd2/main/dxx-redux-d2-headless-route.exe'
$hog = Join-Path $repoRoot 'game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data'
foreach ($run in 1..2) {
    $userDir = Join-Path $output "visit-user$run"
    New-Item -ItemType Directory -Path $userDir | Out-Null
    Copy-Item -LiteralPath (Join-Path $source "key-user$run/blue.sg0") -Destination $userDir
    $common = @('-hogdir', $hog, '-extra-dir', $stages[0].FullName, '-mission', 'LOSTLVLS', '-route-confirm-user-dir', $userDir)
    $cases = @(
        @{ Name = 'entry'; From = 4; To = -1; Trigger = 20; Input = 'blue.sg0'; Output = 'entered.sg0' },
        @{ Name = 'return'; From = -1; To = 4; Trigger = 0; Input = 'entered.sg0'; Output = 'returned.sg0' },
        @{ Name = 'revisit'; From = 4; To = -1; Trigger = 20; Input = 'returned.sg0'; Output = 'revisited.sg0' }
    )
    foreach ($case in $cases) {
        $inputHash = (Get-FileHash -LiteralPath (Join-Path $userDir $case.Input)).Hash
        if ($case.Name -eq 'revisit') { $secretHash = (Get-FileHash -LiteralPath (Join-Path $userDir 'secret.sgc')).Hash }
        $resultPath = Join-Path $output "$($case.Name)$run.json"
        & $exe @common -level $case.From -route-confirm-checkpoint $case.Input `
            -route-confirm-exit-trigger $case.Trigger -route-confirm-native-transition `
            -route-confirm-checkpoint-out $case.Output -route-confirm-json-out $resultPath *> (Join-Path $output "$($case.Name)$run.log")
        if ($LASTEXITCODE -ne 0) { throw "Native $($case.Name) scenario failed" }
        $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
        if ($result.status -ne 'confirmed' -or $result.frames -eq 0 -or
            $result.verification_goal.kind -ne 'exit_trigger' -or $result.verification_goal.trigger -ne $case.Trigger -or
            $result.native_transition.from_level -ne $case.From -or $result.native_transition.to_level -ne $case.To -or
            $result.native_transition.key_flags -ne 2 -or $result.start_state.key_flags -ne 2 -or
            $result.objectives[-1].activation_kind -ne 'enter_exit' -or $null -ne $result.route_confirmation -or
            $result.radius.player -ne $result.radius.effective) {
            throw 'Exit must physically complete and transition with only the earned blue key, without certifying the mine'
        }
        if ((Get-Item -LiteralPath (Join-Path $userDir $case.Output)).Length -eq 0 -or
            (Get-FileHash -LiteralPath (Join-Path $userDir $case.Input)).Hash -ne $inputHash) { throw 'Checkpoint handoff failed' }
        if ($case.Name -eq 'entry' -and -not (Test-Path -LiteralPath (Join-Path $userDir 'secret.sgb'))) { throw 'Native base return save missing' }
        if ($case.Name -eq 'return' -and -not (Test-Path -LiteralPath (Join-Path $userDir 'secret.sgc'))) { throw 'Native secret persistence save missing' }
        if ($case.Name -eq 'revisit' -and (Get-FileHash -LiteralPath (Join-Path $userDir 'secret.sgc')).Hash -ne $secretHash) {
            throw 'Revisit replaced its persisted secret input'
        }
    }
}
foreach ($name in 'entry', 'return', 'revisit') {
    if ((Get-FileHash (Join-Path $output "${name}1.json")).Hash -ne (Get-FileHash (Join-Path $output "${name}2.json")).Hash) {
        throw "$name scenario repeats differ"
    }
}
Write-Host 'PASS Lost Levels: earned-blue entry, native return save, and repeated secret revisit; no completed-mine claim'
exit 0
