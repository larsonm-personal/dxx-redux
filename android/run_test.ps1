#!/usr/bin/env pwsh
# run_test.ps1 -- Run an automation test script on the Android emulator with health checks.
#
# Usage:
#   .\run_test.ps1 test_death.json5
#   .\run_test.ps1 test_death.json5 -Install   # also install APK first
#   .\run_test.ps1 test_axis_mapping.json5 -Game d1  # force a specific game
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
    [int]$TimeoutSeconds = 300,
    [ValidateSet("d1","d2")]
    [string]$Game
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\test_helpers.ps1"

# -- Step 1: Health check -------------------------------------

Ensure-EmulatorHealthy

# -- Step 2: Install APK if requested ------------------------

if ($Install) {
    $apk = "$PSScriptRoot\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "FAIL: APK not found at $apk" "Red"
        exit 1
    }
    Write-Status "Installing APK..."
    Adb -AdbArgs @("install", "-r", $apk) | Write-Host
}

# -- Step 3: Determine which game(s) to run -------------------

$scriptPath = Join-Path "$PSScriptRoot\game_scripts" $ScriptName
$gameList = Get-ScriptGameInfo -ScriptPath $scriptPath

if ($Game) {
    # Explicit -Game parameter overrides _info
    $gameList = @($Game)
} elseif (-not $gameList) {
    # Fallback: legacy _d1_ filename heuristic
    if ($ScriptName -match '_d1_') {
        $gameList = @("d1")
    } else {
        $gameList = @("d2")
    }
}

if ($gameList.Count -gt 1) {
    Write-Status "Multi-game script -- will run for: $($gameList -join ', ')"
}

# -- Run for each game ---------------------------------------

$allPassed = $true
foreach ($gameId in $gameList) {
    if ($gameList.Count -gt 1) {
        Write-Host ""
        Write-Host "============================================================" -ForegroundColor White
        Write-Host "  Running as $($gameId.ToUpper())" -ForegroundColor White
        Write-Host "============================================================" -ForegroundColor White
    }

    # -- Resolve script (variable substitution + conditional filtering) --

    $resolvedPath = Resolve-TestScript -ScriptPath $scriptPath -GameId $gameId
    $pushName = $ScriptName
    if ($resolvedPath -ne $scriptPath) {
        # Push the resolved file instead, but keep the original name on device
        $pushSrc = $resolvedPath
    } else {
        $pushSrc = $scriptPath
    }
    Write-Status "Pushing test script: $ScriptName"
    Adb -AdbArgs @("push", $pushSrc, "/data/local/tmp/$pushName") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/$pushName", "files/$pushName") | Out-Null

    # -- Launch game with verification ----------------------------

    $extraArgs = @()
    if ($gameId -eq "d1") {
        Write-Status "Launching as D1"
        $extraArgs = @("--es", "game", "d1")
    }

    $preLaunch = {
        Reset-GameState
    }

    if (-not (Start-GameWithRetry -ExtraLaunchArgs $extraArgs -PreLaunchScript $preLaunch -Game $gameId)) {
        $allPassed = $false
        if ($gameList.Count -gt 1) { Write-Status "FAIL for $($gameId.ToUpper())" "Red"; continue }
        exit 1
    }

    # -- Send automation broadcast -------------------------------

    Start-Sleep -Seconds 2
    Adb -AdbArgs @("logcat", "-c") | Out-Null
    # Remove stale result files so Watch-AutomationResult doesn't read a prior run's PASS
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_log.jsonl") | Out-Null
    Write-Status "Sending automation broadcast for: $ScriptName"
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", $ScriptName) | Out-Null

    # -- Step 6: Monitor with health checks ----------------------

    # Calculate timeout from script timing fields (or use explicit -TimeoutSeconds)
    $scriptTimeout = $TimeoutSeconds
    if ($TimeoutSeconds -eq 300) {
        # Default -- calculate from script content instead
        $srcForTimeout = if ($resolvedPath -ne $scriptPath) { $resolvedPath } else { $scriptPath }
        $scriptTimeout = Get-ScriptTimeoutSeconds -ScriptPath $srcForTimeout
        Write-Status "Calculated timeout: ${scriptTimeout}s (from script timing fields)"
    }

    $passed = Watch-AutomationResult -TimeoutSeconds $scriptTimeout
    if (-not $passed) {
        $allPassed = $false
        if ($gameList.Count -gt 1) { Write-Status "FAIL for $($gameId.ToUpper())" "Red"; continue }
        exit 1
    }

    # -- Step 7: Dump introspection -------------------------------

    Write-Status "Dumping final introspection state..."
    $intro = Get-GameIntrospection
    if ($intro) {
        Write-Host ($intro | ConvertTo-Json -Depth 10 -Compress)
    }

    if ($gameList.Count -gt 1) {
        Write-Status "PASS for $($gameId.ToUpper())" "Green"
        # Force-stop between game runs
        Adb -AdbArgs @("shell", "am", "force-stop", $script:PACKAGE) | Out-Null
        Start-Sleep -Seconds 2
    }
}

if (-not $allPassed) { exit 1 }
exit 0
