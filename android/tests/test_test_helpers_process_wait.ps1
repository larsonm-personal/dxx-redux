#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\..\helpers\test_helpers.ps1"

$helperSource = Get-Content "$PSScriptRoot\..\helpers\test_helpers.ps1" -Raw
if ($helperSource -notmatch '\$devices\s*=\s*Adb-Timeout\s+-AdbArgs\s+"devices"\s+-Seconds\s+5') {
    throw 'Emulator health device-list probe is not bounded'
}
if ($helperSource -notmatch 'return \(Adb-Timeout -AdbArgs \$AdbArgs -Seconds \$Seconds -IncludeStandardError\)') {
    throw 'Default ADB helper is not bounded'
}
if ($helperSource -notmatch 'return \(Adb-Dev-Timeout -Serial \$Serial -AdbArgs \$AdbArgs -Seconds \$Seconds -IncludeStandardError\)') {
    throw 'Default device-targeted ADB helper is not bounded'
}
if ($helperSource -notmatch 'Adb-Timeout -AdbArgs "start-server" -Seconds 10') {
    throw 'ADB server restart is not bounded'
}

$matchingResult = '{"result":"PASS","run_id":"run-a"}' | ConvertFrom-Json
$otherResult = '{"result":"PASS","run_id":"run-b"}' | ConvertFrom-Json
$legacyResult = '{"result":"PASS"}' | ConvertFrom-Json

if (-not (Test-AutomationResultRunId -Result $matchingResult -ExpectedRunId "run-a")) {
    throw 'Matching automation run id was rejected'
}
if (Test-AutomationResultRunId -Result $otherResult -ExpectedRunId "run-a") {
    throw 'Mismatched automation run id was accepted'
}
if (Test-AutomationResultRunId -Result $legacyResult -ExpectedRunId "run-a") {
    throw 'Missing automation run id was accepted by a correlated runner'
}
if (-not (Test-AutomationResultRunId -Result $legacyResult -ExpectedRunId "")) {
    throw 'Legacy uncorrelated result was rejected'
}

$expectedStatuses = @{
    "0:False:False" = "PASS"
    "1:False:False" = "FAIL"
    "2:False:False" = "FAIL"
    "2:False:True" = "SKIP"
    "0:True:False" = "TIMEOUT"
}
foreach ($case in $expectedStatuses.GetEnumerator()) {
    $parts = $case.Key -split ":"
    $actual = Get-TestStatusFromExitCode `
        -ExitCode ([int]$parts[0]) `
        -TimedOut ([bool]::Parse($parts[1])) `
        -SkipDeclared ([bool]::Parse($parts[2]))
    if ($actual -ne $case.Value) {
        throw "Exit status mapping for $($case.Key) was $actual, expected $($case.Value)"
    }
}

$script:HealthChecks = [System.Collections.Generic.Queue[bool]]::new()
$script:AdbRestarts = 0
function Test-EmulatorHealthy {
    return $script:HealthChecks.Dequeue()
}
function Restart-AdbServer {
    $script:AdbRestarts++
}

$script:HealthChecks.Enqueue($false)
$script:HealthChecks.Enqueue($true)
if (-not (Confirm-EmulatorHealthWithAdbRecovery -RetryDelayMilliseconds 0)) {
    throw 'Transient emulator health failure was not retried'
}
if ($script:AdbRestarts -ne 0) {
    throw 'ADB was restarted when the emulator recovered on retry'
}

$script:HealthChecks.Enqueue($false)
$script:HealthChecks.Enqueue($false)
$script:HealthChecks.Enqueue($true)
if (-not (Confirm-EmulatorHealthWithAdbRecovery -RetryDelayMilliseconds 0)) {
    throw 'Emulator health was not rechecked after ADB restart'
}
if ($script:AdbRestarts -ne 1) {
    throw "Expected one ADB restart, got $script:AdbRestarts"
}

$script:HealthChecks.Enqueue($false)
$script:HealthChecks.Enqueue($false)
$script:HealthChecks.Enqueue($false)
if (Confirm-EmulatorHealthWithAdbRecovery -RetryDelayMilliseconds 0) {
    throw 'Persistent emulator health failure was accepted after ADB restart'
}
if ($script:AdbRestarts -ne 2) {
    throw "Expected two total ADB restarts, got $script:AdbRestarts"
}

$script:TimeoutCalls = 0
$script:GameProcessAlive = $true

function Adb-Timeout {
    param([string[]]$AdbArgs, [int]$Seconds = 8, [switch]$IncludeStandardError)
    $script:TimeoutCalls++
    $name = $AdbArgs[-1]
    if ($name -eq 'pwd') {
        return '/data/user/0/com.dxxredux.app'
    }
    if ($AdbArgs -contains 'cat' -and $name -eq 'files/d1x-redux/.active_set_path') {
        return '/data/user/0/com.dxxredux.app/files/imported/sets/default'
    }
    if ($AdbArgs -contains 'cat' -and $name -eq 'files/d2x-redux/.active_set_path') {
        return '/data/user/0/com.dxxredux.app/files/imported/sets/default'
    }
    if ($name -eq "$($script:PACKAGE):game" -and $script:GameProcessAlive) {
        return '12345'
    }
    return ''
}

if (Wait-ProcessDead -TimeoutMs 250) {
    throw 'Wait-ProcessDead returned true while the game process was still alive'
}
if ($script:TimeoutCalls -lt 2) {
    throw 'Wait-ProcessDead did not query both app processes'
}

$script:GameProcessAlive = $false
if (-not (Wait-ProcessDead -TimeoutMs 250)) {
    throw 'Wait-ProcessDead did not return true after both processes were gone'
}

$script:ResetEvents = @()
function Stop-AppAndWait {
    $script:ResetEvents += "stop:$env:ANDROID_SERIAL"
}
function Adb {
    param([string[]]$AdbArgs)
    $script:ResetEvents += "adb:$($AdbArgs -join ' ')"
    return ''
}

$savedSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = 'original-serial'
try {
    Reset-DeviceGameState -Serial 'test-serial'
    if ($env:ANDROID_SERIAL -ne 'original-serial') {
        throw 'Reset-DeviceGameState did not restore ANDROID_SERIAL'
    }
} finally {
    if ($savedSerial) { $env:ANDROID_SERIAL = $savedSerial }
    else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}

if ($script:ResetEvents[0] -ne 'stop:test-serial') {
    throw 'Reset-DeviceGameState did not stop the selected device before cleanup'
}
$resetCommands = $script:ResetEvents -join "`n"
foreach ($requiredArtifact in @(
        'files/mods/mod_manifest.json',
        'files/audio_sources.json',
        'files/audio_playlist.json',
        'files/pending_resume_launch.json',
        'files/automation_result.json',
        'files/automation_log.jsonl',
        'files/file_sets.json',
        'quick_record_sidecar.*'
    )) {
    if ($resetCommands -notmatch [regex]::Escape($requiredArtifact)) {
        throw "Reset-GameState did not clear $requiredArtifact"
    }
}

Write-Host 'PASS'
