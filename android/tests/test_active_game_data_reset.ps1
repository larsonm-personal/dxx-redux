#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

. "$PSScriptRoot\..\helpers\test_helpers.ps1"

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$script:appDataDir = "/data/user/0/com.dxxredux.app"
$script:expectedDefault = "$($script:appDataDir)/files/imported/sets/default"
$script:markers = @{
    "files/d1x-redux/.active_set_path" = "/stale/set"
    "files/d2x-redux/.active_set_path" = "/stale/set"
}
$script:presentHogs = @{
    "descent.hog" = $true
    "descent2.hog" = $true
}
$script:ignorePublishFor = $null
$script:adbCalls = @()
$script:statusMessages = @()

function Write-Status {
    param([string]$Message, [string]$Color = "White")
    $script:statusMessages += $Message
}

function Adb-Timeout {
    param([string[]]$AdbArgs, [int]$Seconds = 8, [switch]$IncludeStandardError)
    $script:adbCalls += , @($AdbArgs)

    if ($AdbArgs[-1] -eq "pwd") { return $script:appDataDir }
    if ($AdbArgs.Count -ge 2 -and $AdbArgs[-2] -eq "cat") {
        return $script:markers[$AdbArgs[-1]]
    }
    if ($AdbArgs -contains "mv") {
        foreach ($gameDir in @("d1x-redux", "d2x-redux")) {
            if ($AdbArgs[-1] -ceq "files/$gameDir/.active_set_path" -and $script:ignorePublishFor -cne $gameDir) {
                $script:markers[$AdbArgs[-1]] = $script:expectedDefault
            }
        }
    }
    if ($AdbArgs -contains "stat") {
        foreach ($hog in $script:presentHogs.Keys) {
            if ($AdbArgs[-1] -match [regex]::Escape("/$hog") -and $script:presentHogs[$hog]) { return "42" }
        }
    }
    return $null
}

Assert-True (Publish-DefaultActiveFileSet) "Publishing the default active set should succeed"
Assert-True ($script:markers["files/d1x-redux/.active_set_path"] -ceq $script:expectedDefault) "D1 marker was not updated"
Assert-True ($script:markers["files/d2x-redux/.active_set_path"] -ceq $script:expectedDefault) "D2 marker was not updated"
$copyCalls = @($script:adbCalls | Where-Object { $_ -contains "cp" -and $_[-1] -match '\.active_set_path\.tmp$' })
$moveCalls = @($script:adbCalls | Where-Object { $_ -contains "mv" -and $_[-1] -match '\.active_set_path$' })
Assert-True ($copyCalls.Count -eq 2) "Expected one temporary marker copy per engine"
Assert-True ($moveCalls.Count -eq 2) "Expected one atomic marker rename per engine"

Assert-True (Test-StandardGameDataActive) "Valid active standard game data should pass preflight"
$script:markers["files/d1x-redux/.active_set_path"] = "/stale/set"
Assert-True (-not (Test-StandardGameDataActive)) "A stale active-set marker should fail preflight"

$script:markers["files/d1x-redux/.active_set_path"] = $script:expectedDefault
$script:presentHogs["descent2.hog"] = $false
Assert-True (-not (Test-StandardGameDataActive)) "A missing D2 base HOG should fail preflight"

$script:presentHogs["descent2.hog"] = $true
$script:markers["files/d2x-redux/.active_set_path"] = "/stale/set"
$script:ignorePublishFor = "d2x-redux"
Assert-True (-not (Publish-DefaultActiveFileSet)) "Publication must fail when a marker cannot be verified"
Assert-True (($script:statusMessages -join "`n") -match 'Expected marker:' -and
    ($script:statusMessages -join "`n") -match 'Actual marker:') "Publication failure did not retain marker diagnostics"

Assert-True (Test-StandardGameDataFailureReason -Reason "could not find descent.hog") "D1 base-data failures should be recognized"
Assert-True (Test-StandardGameDataFailureReason -Reason "problems=could not find descent2.hog or d2demo.hog") "D2 base-data failures should be recognized"
Assert-True (-not (Test-StandardGameDataFailureReason -Reason "metadata output file was not created")) "Unrelated failures must not trigger the base-data circuit breaker"

$script:publishAttempts = 0
$script:recoveryAttempts = 0
function Publish-DefaultActiveFileSet {
    $script:publishAttempts++
    return ($script:publishAttempts -ge 2)
}
function Ensure-EmulatorHealthy {
    $script:recoveryAttempts++
    return $true
}
Reset-GameState
Assert-True ($script:publishAttempts -eq 2) "Reset did not retry active-set publication"
Assert-True ($script:recoveryAttempts -eq 1) "Reset did not recover the emulator before retrying publication"

Write-Host "Active game-data reset tests passed" -ForegroundColor Green
exit 0
