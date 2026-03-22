<#
.SYNOPSIS
    Verifies that double-launching the game (Home then re-launch) does
    not crash.  Regression test for the SIGSEGV caused by two game
    threads running simultaneously in the :game process.
.DESCRIPTION
    Phase 1: Launch game via the launcher broadcast
    Phase 2: Press Home to background the game (process stays alive)
    Phase 3: Simulate launcher edit (write_autoselect broadcast)
    Phase 4: Re-launch game -- should NOT crash (guard prevents it)
    Phase 5: Verify the original game process is still alive and healthy
#>
param(
    [string]$Game = "d2"
)

$ErrorActionPreference = "Continue"
. "$PSScriptRoot\..\test_helpers.ps1"

Ensure-EmulatorHealthy

$outFile = Join-Path $PSScriptRoot "..\..\temp\test_double_launch_result.txt"
"" | Out-File $outFile

function Log($msg) {
    $ts = Get-Date -Format "HH:mm:ss"
    $line = "[$ts] $msg"
    Write-Host $line
    $line | Out-File $outFile -Append
}

function GetGamePid {
    $result = Adb-Timeout -AdbArgs @("shell", "pidof", "com.dxxredux.app:game") -Seconds 5
    if ($result) { return $result.Trim() } else { return "" }
}

# Clean state
Log "=== Double-launch crash test (game=$Game) ==="
Log "Force-stopping app..."
Adb-Timeout -AdbArgs @("shell", "am", "force-stop", "com.dxxredux.app") -Seconds 10 | Out-Null
Start-Sleep -Seconds 2
Adb -AdbArgs @("logcat", "-c") | Out-Null

# Phase 1: Start the game
Log "Phase 1: Starting game..."
Adb -AdbArgs @("shell", "am", "start-activity", "-n", "com.dxxredux.app/.SetupActivity") | Out-Null
Start-Sleep -Seconds 4
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND", "--es", "command", "launch", "--es", "game", $Game) | Out-Null
Start-Sleep -Seconds 15

$pid1 = GetGamePid
Log "Game PID after first launch: $pid1"

if (-not $pid1) {
    Log "FAIL: Game did not start"
    exit 1
}

# Verify game is on a menu screen
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.INTROSPECT") | Out-Null
Start-Sleep -Seconds 1
$json = Adb-Timeout -AdbArgs @("shell", "run-as", "com.dxxredux.app", "cat", "files/introspect.json") -Seconds 5
$screenMode = if ($json -match '"screen_mode":"([^"]+)"') { $Matches[1] } else { "unknown" }
Log "Game screen mode: $screenMode"

if ($screenMode -ne "menu") {
    Log "WARN: Expected screen_mode=menu, got $screenMode"
}

# Phase 2: Press Home to background the game (NOT quit)
Log "Phase 2: Pressing HOME to background game..."
Adb -AdbArgs @("shell", "input", "keyevent", "KEYCODE_HOME") | Out-Null
Start-Sleep -Seconds 3

$pid2 = GetGamePid
Log "Game PID after HOME: $pid2"
if (-not $pid2) {
    Log "FAIL: Game process died after HOME -- cannot test double-launch"
    exit 1
}
if ($pid2 -ne $pid1) {
    Log "FAIL: PID changed after HOME ($pid1 -> $pid2)"
    exit 1
}
Log "OK: process stayed alive after HOME"

# Phase 3: Simulate launcher autoselect edit
Log "Phase 3: Writing autoselect orderings via broadcast..."
Adb -AdbArgs @("shell", "am", "start-activity", "-n", "com.dxxredux.app/.SetupActivity") | Out-Null
Start-Sleep -Seconds 3
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
    "--es", "command", "write_autoselect", "--es", "game", $Game,
    "--es", "primary", "8,9,7,6,5,4,3,2,1,0,255",
    "--es", "secondary", "9,8,4,3,1,5,0,255,7,6,2") | Out-Null
Start-Sleep -Seconds 2

$pid3 = GetGamePid
Log "Game PID after autoselect write: $pid3"
if (-not $pid3) {
    Log "FAIL: Game died during autoselect write"
    exit 1
}

# Phase 4: Attempt to re-launch game (should be blocked by guard)
Log "Phase 4: Re-launching game via broadcast..."
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND", "--es", "command", "launch", "--es", "game", $Game) | Out-Null
Start-Sleep -Seconds 10

$pid4 = GetGamePid
Log "Game PID after re-launch attempt: $pid4"

if (-not $pid4) {
    Log "FAIL: Game process DIED -- double-launch crash!"
    Log "--- Logcat crash info ---"
    $crash = Adb-Timeout -AdbArgs @("logcat", "-d", "-s", "AndroidRuntime:E", "DEBUG:I") -Seconds 5
    if ($crash) { ($crash -split "`n") | Select-Object -Last 30 | ForEach-Object { Log $_ } }
    exit 1
}

if ($pid4 -ne $pid1) {
    Log "FAIL: PID changed ($pid1 -> $pid4) -- guard did not prevent re-launch"
    exit 1
}
Log "OK: Same PID -- double-launch guard worked"

# Phase 5: Verify game is still healthy
Log "Phase 5: Verifying game health..."

# Bring the game back to foreground so introspection works
Adb-Timeout -AdbArgs @("shell", "monkey", "-p", "com.dxxredux.app",
    "-c", "android.intent.category.LAUNCHER", "1") -Seconds 5 | Out-Null
Start-Sleep -Seconds 2
Adb -AdbArgs @("shell", "input", "keyevent", "KEYCODE_BACK") | Out-Null
Start-Sleep -Seconds 2

Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.INTROSPECT") | Out-Null
Start-Sleep -Seconds 1
$json2 = Adb-Timeout -AdbArgs @("shell", "run-as", "com.dxxredux.app", "cat", "files/introspect.json") -Seconds 5
$screenMode2 = if ($json2 -match '"screen_mode":"([^"]+)"') { $Matches[1] } else { "unknown" }
Log "Game screen mode after test: $screenMode2"

$pid5 = GetGamePid
if (-not $pid5) {
    Log "FAIL: Game died during final health check"
    exit 1
}
if ($pid5 -ne $pid1) {
    Log "FAIL: PID changed during health check ($pid1 -> $pid5)"
    exit 1
}

Log "PASS: Double-launch guard prevented crash, game still running (PID $pid1)"
Log "=== Test complete ==="
exit 0
