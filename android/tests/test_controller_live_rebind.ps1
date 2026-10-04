#!/usr/bin/env pwsh
# Verify launcher edits reach the same running engine on return to the game
param(
    [ValidateSet('d1', 'd2')][string]$Game = 'd2',
    [string]$Serial
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$Serial = Initialize-AndroidTestTarget -Serial $Serial
$evidenceDir = Join-Path $script:REPO_ROOT 'temp'
$baselineLog = Join-Path $evidenceDir "controller-live-rebind-$Game-baseline.log"
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts @($baselineLog)
New-Item -ItemType Directory -Path $evidenceDir -Force | Out-Null

# Keep baseline and return verification in one process so runner cleanup waits
Adb -AdbArgs @('logcat', '-c') | Out-Null
& "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_independent_trigger_axes.jsonc -Game $Game -LeaveRunning -TimeoutSeconds 180 *> $baselineLog
if ($LASTEXITCODE -ne 0) { throw "Baseline failed; see $baselineLog" }
$gamePidBefore = (Adb -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game")).Trim()
if (-not $gamePidBefore) { throw 'Baseline did not retain the game process' }

function Push-RebindFixture([string]$Name) {
    $fixture = Join-Path $PSScriptRoot "../test_fixtures/controller_live_rebind/$Name.json"
    $remote = "/data/local/tmp/controller_live_rebind_$Name.json"
    $private = "files/controller_live_rebind_$Name.json"
    Adb -AdbArgs @('push', $fixture, $remote) | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', $remote, $private) | Out-Null
    return "controller_live_rebind_$Name.json"
}

function Wait-RebindResult([string]$RunId) {
    $deadline = [DateTime]::UtcNow.AddSeconds(60)
    while ([DateTime]::UtcNow -lt $deadline) {
        $raw = Adb-Timeout -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/automation_result.json') -Seconds 5
        if ($raw) {
            $result = $raw | ConvertFrom-Json
            if ($result.run_id -eq $RunId) {
                if ($result.result -ne 'PASS') { throw "Rebind failed: $raw" }
                Write-Host "PASS: rebind fixture ($($result.steps_completed)/$($result.total_steps) steps)"
                return
            }
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Timed out waiting for rebind run $RunId"
}

$launcherScript = Push-RebindFixture 'launcher'
$gameScript = Push-RebindFixture 'game'
Adb -AdbArgs @('shell', 'am', 'start', '-f', '0x20000', '-n', "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
$launcherRunId = [guid]::NewGuid().ToString('N')
Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_AUTOMATE', '--es', 'script', $launcherScript, '--es', 'run_id', $launcherRunId) | Out-Null
Wait-RebindResult $launcherRunId

$gamePidAfter = (Adb -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game")).Trim()
if ($gamePidAfter -ne $gamePidBefore) { throw "Game process changed: $gamePidBefore -> $gamePidAfter" }
$gameRunId = [guid]::NewGuid().ToString('N')
Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', $gameScript, '--es', 'run_id', $gameRunId) | Out-Null
Wait-RebindResult $gameRunId
Write-Host "PASS: $Game retained process $gamePidBefore, reloaded native bindings, and fired/released L2/R2"
