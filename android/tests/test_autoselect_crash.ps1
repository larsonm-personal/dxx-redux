<#
.SYNOPSIS
    Reproduces the crash that occurs after launcher-based autoselect
    ordering save followed by opening nested menus.
.DESCRIPTION
    Phase 1: Launch game once to create a pilot, then exit
    Phase 2: Patch the .plr file (simulate launcher autoselect save)
    Phase 3: Relaunch game and navigate to nested menus, check for crash
#>
param(
    [string]$Game = "d2"
)

$ErrorActionPreference = "Continue"
. "$PSScriptRoot\..\test_helpers.ps1"

Ensure-EmulatorHealthy

# ===== Phase 1: Create a pilot =====
Write-Status "=== Phase 1: Create pilot ==="

# Push a tiny script that just accepts the default pilot name and waits at main menu
$createPilotScript = @'
[
    {"_info": {"games": ["d2"]}},
    {"action": "log", "message": "create pilot then quit gracefully"},
    {"action": "wait_ms", "ms": 2000},
    {"action": "wait_for", "field": "screen_mode", "value": "menu", "timeout_ms": 20000},
    {"action": "select", "text": "Ok", "post_delay_ms": 2000},
    {"action": "log", "message": "pilot accepted, quitting gracefully"},
    {"action": "select", "text": "Quit", "post_delay_ms": 500}
]
'@
$createPilotScriptPath = Join-Path $env:TEMP "create_pilot_quit.json5"
Set-Content -Path $createPilotScriptPath -Value $createPilotScript -Encoding UTF8

# Delete any existing pilot files for a clean start
$preLaunch = {
    Reset-GameState
}

if (-not (Start-GameWithRetry -PreLaunchScript $preLaunch -Game $Game)) {
    Write-Status "FAIL: Could not start game for Phase 1" "Red"
    exit 1
}

# Push and run the create-pilot script
Write-Status "Pushing create-pilot script..."
Adb -AdbArgs @("push", $createPilotScriptPath, "/data/local/tmp/create_pilot_quit.json5") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/create_pilot_quit.json5", "files/create_pilot_quit.json5") | Out-Null

Start-Sleep -Seconds 2
Adb -AdbArgs @("logcat", "-c") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null
Write-Status "Sending create-pilot automation..."
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", "create_pilot_quit.json5") | Out-Null

$passed = Watch-AutomationResult -TimeoutSeconds 60
# Game exits after Quit without writing result - just wait for process death
$deadline = (Get-Date).AddSeconds(30)
$gameExited = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    $gameProc = & $script:ADB shell pidof "com.dxxredux.app:game" 2>$null | Out-String
    if (-not $gameProc.Trim()) {
        $gameExited = $true
        break
    }
}
if (-not $gameExited -and -not $passed) {
    Write-Status "FAIL: Pilot creation did not complete (game still running)" "Red"
    exit 1
}
Write-Status "Game process exited (graceful quit) - Phase 1 done"

# Wait and force-stop any remaining processes (launcher process)
Start-Sleep -Seconds 3
Write-Status "Force-stopping remaining app processes..."
Adb -AdbArgs @("shell", "am", "force-stop", $script:PACKAGE) | Out-Null
Start-Sleep -Seconds 2

# ===== Phase 2: Patch the .plr file via JNI (actual launcher path) =====
Write-Status "=== Phase 2: Patch .plr file via JNI autoselect save ==="

# Need SetupActivity running for the broadcast receiver
Write-Status "Starting SetupActivity for JNI patch..."
Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
Start-Sleep -Seconds 5

# Trigger the actual JNI autoselect save (same code path as the save button)
# Swap first two primary weapons from default: 8,9,... instead of 9,8,...
$primStr = "8,9,7,6,5,4,3,2,1,0,255"
$secStr = "9,8,4,3,1,5,0,255,7,6,2"
Write-Status "Sending write_autoselect broadcast (game=$Game)..."
$broadcastResult = Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
    "--es", "command", "write_autoselect",
    "--es", "game", $Game,
    "--es", "primary", $primStr,
    "--es", "secondary", $secStr) | Out-String
Write-Status "Broadcast result: $($broadcastResult.Trim())"
Start-Sleep -Seconds 2

# Verify the patch took effect via logcat
$logResult = Adb -AdbArgs @("logcat", "-d", "-t", "10", "-s", "DXX-Setup:I") | Out-String
Write-Status "Logcat: $($logResult.Trim())"

# ===== Phase 3: Relaunch and navigate menus =====
Write-Status "=== Phase 3: Launch game with patched .plr, navigate menus ==="

# Push the menu navigation script
$menuScriptSrc = Join-Path $PSScriptRoot "..\game_scripts\test_autoselect_crash_repro.json5"

# Launch game WITHOUT deleting pilots (critical: pilot file must persist)
$preLaunch2 = {
    # Do NOT delete .plr files! We need the patched pilot.
    # Only remove stale automation results
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_log.jsonl") | Out-Null
}

if (-not (Start-GameWithRetry -PreLaunchScript $preLaunch2 -Game $Game)) {
    Write-Status "FAIL: Could not start game for Phase 3" "Red"
    exit 1
}

# Push and run the menu navigation script
Write-Status "Pushing menu navigation script..."
Adb -AdbArgs @("push", $menuScriptSrc, "/data/local/tmp/test_autoselect_crash_repro.json5") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/test_autoselect_crash_repro.json5", "files/test_autoselect_crash_repro.json5") | Out-Null

Start-Sleep -Seconds 2
Adb -AdbArgs @("logcat", "-c") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null
Write-Status "Sending menu navigation automation..."
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", "test_autoselect_crash_repro.json5") | Out-Null

$passed = Watch-AutomationResult -TimeoutSeconds 120
if (-not $passed) {
    Write-Status "FAIL: Menu navigation crashed or timed out" "Red"
    # Dump diagnostics
    $log = Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_log.jsonl")
    Write-Host "=== Automation log ===" -ForegroundColor Yellow
    Write-Host $log
    $gamePid = Adb -AdbArgs @("shell", "pidof", "$($script:PACKAGE):game") 2>$null
    if ($gamePid -and "$gamePid".Trim()) {
        Write-Status "Game process alive: PID $("$gamePid".Trim())"
    } else {
        Write-Status "Game process: DEAD (crashed!)" "Red"
    }
    exit 1
}

# Check game is still alive
$gamePid = Adb -AdbArgs @("shell", "pidof", "$($script:PACKAGE):game") 2>$null
if ($gamePid -and "$gamePid".Trim()) {
    Write-Status "Game process alive: PID $("$gamePid".Trim())" "Green"
} else {
    Write-Status "Game process: DEAD (crashed after test completion!)" "Red"
    exit 1
}

$result = Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/automation_result.json")
Write-Status "Automation result: $result"
Write-Status "PASS" "Green"
exit 0
