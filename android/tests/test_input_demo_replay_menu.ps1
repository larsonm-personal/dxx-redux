#!/usr/bin/env pwsh

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$androidRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $androidRoot 'helpers/input_demo_replay_menu.ps1')
. (Join-Path $androidRoot 'helpers/run_all_tests_profile_menu.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

Assert-True ((Select-RunAllTestsProfile -ReadChoice { 'r' }) -eq 'ReplayDemo') 'Suite menu should select demo replay'
foreach ($case in @(@('1', 'd1'), @('2', 'd2'), @('3', 'd1-in-d2'), @('', 'd1'), @('Q', 'cancel'))) {
    Assert-True ((Select-InputDemoReplayEngine -ReadChoice { $case[0] }) -eq $case[1]) 'Incorrect engine selection'
}
foreach ($case in @(@('1', 'headed'), @('2', 'headless'), @('', 'headed'), @('Q', 'cancel'))) {
    Assert-True ((Select-InputDemoReplayDisplay -ReadChoice { $case[0] }) -eq $case[1]) 'Incorrect playback selection'
}
$choices = [System.Collections.Generic.Queue[string]]::new()
$choices.Enqueue('invalid')
$choices.Enqueue('2')
Assert-True ((Select-InputDemoReplayDisplay -ReadChoice { $choices.Dequeue() }) -eq 'headless') 'Invalid choices should retry'

# Exercise the real wrapper's header-based filtering and the suite's early
# cancellation path, without starting a game, build or test infrastructure
function Read-Host {
    param([string]$Prompt)
    if ($menuTestAnswers.Count -eq 0) { throw "Unexpected prompt: $Prompt" }
    return $menuTestAnswers.Dequeue()
}

$fixtureRoot = Join-Path $androidRoot 'temp/input_demo_menu_test'
& (Join-Path $androidRoot 'helpers/retain-recent-artifacts.ps1') -Artifacts $fixtureRoot
New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
try {
    foreach ($game in @('d1', 'd2')) {
        # Names deliberately do not encode the recorded game
        $fileName = if ($game -eq 'd1') { 'first.dximdemo' } else { 'second.dximdemo' }
        @{ type = 'header'; game = $game; mission = 'test'; level = 1; start_mode = 'level_start'; frame_count = 1 } |
            ConvertTo-Json -Compress | Set-Content -LiteralPath (Join-Path $fixtureRoot $fileName) -Encoding utf8
    }
    foreach ($engine in @('1', '2', '3')) {
        $menuTestAnswers = [System.Collections.Generic.Queue[string]]::new()
        $menuTestAnswers.Enqueue($engine)
        $output = (& (Join-Path $PSScriptRoot 'run_input_demo_replay.ps1') -Interactive -ListOnly -SearchRoot $fixtureRoot 6>&1 | Out-String)
        Assert-True ($LASTEXITCODE -eq 0) 'Interactive inventory should succeed'
        $wanted = if ($engine -eq '2') { 'second.dximdemo' } else { 'first.dximdemo' }
        $unwanted = if ($engine -eq '2') { 'first.dximdemo' } else { 'second.dximdemo' }
        Assert-True ($output.Contains($wanted) -and -not $output.Contains($unwanted)) "Wrong recording filter for engine $engine"
    }
    $menuTestAnswers = [System.Collections.Generic.Queue[string]]::new()
    $menuTestAnswers.Enqueue('q')
    $output = (& (Join-Path $androidRoot 'run_all_tests.ps1') -ReplayDemo 6>&1 | Out-String)
    Assert-True ($LASTEXITCODE -eq 0 -and $menuTestAnswers.Count -eq 0) 'Suite replay cancellation should exit successfully'
    Assert-True (-not $output.Contains('Extended sampling:')) 'Replay must not fall through into the suite'
} finally {
    foreach ($name in @('first.dximdemo', 'second.dximdemo')) {
        Remove-Item -LiteralPath (Join-Path $fixtureRoot $name) -Force -ErrorAction SilentlyContinue
    }
}

Write-Host 'PASS: input-demo replay menus and filtering'
