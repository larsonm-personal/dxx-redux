#!/usr/bin/env pwsh
# teardown_docker_nat.ps1 -- Stop Docker NAT containers and clear STUN
# overrides on both emulators.
#
# Usage:
#   .\teardown_docker_nat.ps1

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$ANDROID_ROOT = Split-Path $PSScriptRoot
$REPO_ROOT = Split-Path $ANDROID_ROOT
$DEP_BASE = (Get-Content (Join-Path $REPO_ROOT "dependency_base.txt") -First 1).Trim()
$ADB = "$DEP_BASE\android-sdk\platform-tools\adb.exe"
$COMPOSE_DIR = Join-Path $REPO_ROOT "android\docker\nat-testbed"

$EMU1 = "emulator-5554"
$EMU2 = "emulator-5556"

function Write-Status {
    param([string]$Msg, [string]$Color = "Cyan")
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg" -ForegroundColor $Color
}

# -- Clear STUN overrides on emulators --
Write-Status "Clearing STUN overrides on emulators..."
foreach ($serial in @($EMU1, $EMU2)) {
    $devices = & $ADB devices 2>&1 | Out-String
    if ($devices -match "$serial\s+device") {
        & $ADB -s $serial shell am broadcast -a com.dxxredux.MP_COMMAND `
            --es command stun_override_clear 2>&1 | Out-Null
        Write-Status "  ${serial}: STUN override cleared" "Green"
    } else {
        Write-Status "  ${serial}: not online (skipped)" "Yellow"
    }
}

# -- Stop Docker containers --
Write-Status "Stopping NAT containers..."
Push-Location $COMPOSE_DIR
docker compose down 2>&1 | ForEach-Object { Write-Status "  $_" "Gray" }
Pop-Location

Write-Status "Docker NAT testbed torn down" "Green"
