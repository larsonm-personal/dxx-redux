#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Runs the SAF redbook audio regression script with its required test disc.

.DESCRIPTION
    test_saf_redbook.json5 is a launcher/game automation script, but it needs a
    tiny BIN/CUE fixture staged in two places before the launcher registers the
    audio source.  This wrapper makes that setup explicit so the regression is
    part of the unattended suite instead of sitting behind _standalone=false.
#>
param(
    [int]$TimeoutSeconds = 360
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$androidRoot = Join-Path $repoRoot "android"
$fixtureDir = Join-Path $PSScriptRoot "test_redbook_data"
$binPath = Join-Path $fixtureDir "test_disc.bin"
$cuePath = Join-Path $fixtureDir "test_disc.cue"
$scriptName = "test_saf_redbook.json5"

if (-not (Test-Path -LiteralPath $binPath -PathType Leaf)) {
    Write-Status "FAIL: redbook fixture BIN not found: $binPath" "Red"
    exit 1
}
if (-not (Test-Path -LiteralPath $cuePath -PathType Leaf)) {
    Write-Status "FAIL: redbook fixture CUE not found: $cuePath" "Red"
    exit 1
}

Ensure-EmulatorHealthy

Write-Status "Staging SAF redbook fixture"
Adb -AdbArgs @("push", $binPath, "/data/local/tmp/test_disc.bin") | Out-Null
Adb -AdbArgs @("push", $cuePath, "/data/local/tmp/test_disc.cue") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", "files") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/test_disc.cue", "files/test_disc.cue") | Out-Null
Adb -AdbArgs @("shell", "rm", "-f", "/data/local/tmp/test_disc.cue") | Out-Null

$stagedCue = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "ls", "files/test_disc.cue") -Seconds 5
$stagedBin = Adb-Timeout -AdbArgs @("shell", "ls", "/data/local/tmp/test_disc.bin") -Seconds 5
if (-not $stagedCue -or $stagedCue -match "No such file" -or -not $stagedBin -or $stagedBin -match "No such file") {
    Write-Status "FAIL: could not stage SAF redbook fixture on device" "Red"
    exit 1
}

$runTest = Join-Path (Join-Path $androidRoot "helpers") "run_test.ps1"
& $runTest -ScriptName $scriptName -TimeoutSeconds $TimeoutSeconds -Game d2
$exitCode = $LASTEXITCODE

try {
    Adb -AdbArgs @("shell", "rm", "-f", "/data/local/tmp/test_disc.bin") | Out-Null
} catch {}

exit $exitCode
