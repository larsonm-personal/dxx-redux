#!/usr/bin/env pwsh
# run_test.ps1 -- Run an automation test script on the Android emulator with health checks.
#
# Usage:
#   .\run_test.ps1 test_death_d1_level11.json5
#   .\run_test.ps1 test_death_d1_level11.json5 -Install   # also install APK first
#   .\run_test.ps1 test_death_d1_level11.json5 -Verbose   # show all logcat
#
# The script will:
#   1. Verify emulator is healthy (restart if needed)
#   2. Push the test script to the device
#   3. Launch the app (SetupActivity -> game)
#   4. Wait for the game to start, then send the automation broadcast
#   5. Tail logcat for DXX-Automate, with periodic health checks
#   6. Report PASS/FAIL and exit

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$ScriptName,
    [switch]$Install,
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\test_helpers.ps1"

# ── Step 1: Health check ─────────────────────────────────────

Ensure-EmulatorHealthy

# ── Step 2: Install APK if requested ────────────────────────

if ($Install) {
    $apk = "$PSScriptRoot\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "FAIL: APK not found at $apk" "Red"
        exit 1
    }
    Write-Status "Installing APK..."
    Adb -AdbArgs @("install", "-r", $apk) | Write-Host
}

# ── Step 3: Push test script ────────────────────────────────

if (-not (Send-AutomationScript $ScriptName -PushOnly)) {
    exit 1
}

# ── Step 4: Launch game with verification ────────────────────

$extraArgs = @()
if ($ScriptName -match '_d1_') {
    Write-Status "Detected D1 script -- launching as D1"
    $extraArgs = @("--es", "game", "d1")
}

$preLaunch = {
    # Delete player/config files so tests get fresh defaults
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/player.plr", "files/descent.cfg") | Out-Null
}

if (-not (Start-GameWithRetry -ExtraLaunchArgs $extraArgs -PreLaunchScript $preLaunch)) {
    exit 1
}

# ── Step 5: Send automation broadcast ───────────────────────

Start-Sleep -Seconds 2
Adb -AdbArgs @("logcat", "-c") | Out-Null
Write-Status "Sending automation broadcast for: $ScriptName"
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", $ScriptName) | Out-Null

# ── Step 6: Monitor with health checks ──────────────────────

$passed = Watch-AutomationResult -TimeoutSeconds $TimeoutSeconds
if (-not $passed) { exit 1 }

# ── Step 7: Dump introspection ───────────────────────────────

Write-Status "Dumping final introspection state..."
$intro = Get-GameIntrospection
if ($intro) {
    Write-Host ($intro | ConvertTo-Json -Depth 10 -Compress)
}

exit 0
