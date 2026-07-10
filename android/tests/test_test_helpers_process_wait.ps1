#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

. "$PSScriptRoot\..\helpers\test_helpers.ps1"

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

$script:TimeoutCalls = 0
$script:GameProcessAlive = $true

function Adb-Timeout {
    param([string[]]$AdbArgs, [int]$Seconds = 8)
    $script:TimeoutCalls++
    $name = $AdbArgs[-1]
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

Write-Host 'PASS'
