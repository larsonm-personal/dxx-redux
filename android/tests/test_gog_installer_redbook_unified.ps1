#!/usr/bin/env pwsh
<#
.SYNOPSIS
  GOG Installer + Redbook regression test (unified launcher+game script).

.DESCRIPTION
  Pushes the GOG .exe to the emulator, then invokes run_test.ps1 with the
  unified JSON5 script that handles both launcher setup and in-game verification.

.PARAMETER GogExePath
  Local path to the GOG installer .exe. Defaults to the known location.

.PARAMETER SkipPush
  Skip pushing the GOG .exe (assumes it's already on the emulator).

.EXAMPLE
  .\test_gog_installer_redbook_unified.ps1
  .\test_gog_installer_redbook_unified.ps1 -SkipPush
#>
param(
    [string]$GogExePath,
    [switch]$SkipPush,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\..\test_helpers.ps1"

$GOG_FILENAME = 'setup_descent_2_1.1_(16596).exe'
$DEVICE_GOG_PATH = "/data/local/tmp/$GOG_FILENAME"
$SCRIPT_NAME = 'test_gog_installer_redbook_unified.json5'

# -- Auto-discover GOG .exe path --------------------------------

if (-not $GogExePath) {
    $repoRoot = Split-Path (Split-Path $PSScriptRoot)
    $candidate = Join-Path $repoRoot "game_data\gog installers\$GOG_FILENAME"
    if (Test-Path $candidate) {
        $GogExePath = $candidate
    } else {
        Write-Status "FAIL: GOG .exe not found at $candidate. Pass -GogExePath" "Red"
        exit 1
    }
}

if (-not (Test-Path $GogExePath)) {
    Write-Status "FAIL: GOG .exe not found: $GogExePath" "Red"
    exit 1
}

# -- Push GOG .exe to device ------------------------------------

Ensure-EmulatorHealthy

if (-not $SkipPush) {
    Write-Status "Pushing GOG .exe to device..."
    $localSize = (Get-Item $GogExePath).Length
    $deviceSize = Adb-Timeout -AdbArgs @("shell", "stat -c %s '$DEVICE_GOG_PATH'") -Seconds 5 2>$null
    if ($deviceSize -and $deviceSize -match '^\d+$' -and [long]$deviceSize -eq $localSize) {
        Write-Status "GOG .exe already on device with correct size, skipping push"
    } else {
        Adb -AdbArgs @("push", $GogExePath, $DEVICE_GOG_PATH)
        Write-Status "Push complete"
    }
} else {
    Write-Status "Skipping push (-SkipPush)"
}

# Verify file is on device
$check = Adb-Timeout -AdbArgs @("shell", "ls -la '$DEVICE_GOG_PATH'") -Seconds 5 2>$null
if (-not $check) {
    Write-Status "FAIL: GOG .exe not found on device at $DEVICE_GOG_PATH" "Red"
    exit 1
}

# -- Run unified test via run_test.ps1 --------------------------

$runTest = Join-Path (Split-Path $PSScriptRoot) "run_test.ps1"
& $runTest -ScriptName $SCRIPT_NAME -TimeoutSeconds $TimeoutSeconds -Game d2
exit $LASTEXITCODE
