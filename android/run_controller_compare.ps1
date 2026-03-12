#!/usr/bin/env pwsh
# run_controller_compare.ps1 -- Compare launcher controller config with in-game joystick bindings.
#
# Usage:
#   .\run_controller_compare.ps1
#   .\run_controller_compare.ps1 -Install   # install APK first
#
# Steps:
#   1. Health check (restart emulator if needed)
#   2. Launch SetupActivity, patch pilots, dump launcher controller introspection
#   3. Launch game, run automation script, dump game introspection
#   4. Compare the two joystick_controls sections item by item

param(
    [switch]$Install,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\test_helpers.ps1"

$GAME_SCRIPT = "test_controller_compare.json5"

# ── Step 1: Health check + install ───────────────────────────

Ensure-EmulatorHealthy

if ($Install) {
    $apk = "$PSScriptRoot\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "FAIL: APK not found at $apk" "Red"
        exit 1
    }
    Write-Status "Installing APK..."
    Adb -AdbArgs @("install", "-r", $apk) | Write-Host
}

# ── Step 2: Launch SetupActivity and do pre-game introspection ──

Write-Status "Force-stopping app..."
Adb -AdbArgs @("shell", "am", "force-stop", $PACKAGE) | Out-Null
Start-Sleep -Seconds 2

Adb -AdbArgs @("logcat", "-c") | Out-Null
Write-Status "Launching SetupActivity..."
Adb -AdbArgs @("shell", "am", "start", "-n", "$PACKAGE/$ACTIVITY") | Out-Null

if (-not (Wait-SetupActivityReady)) {
    Write-Status "FAIL: SetupActivity not responding" "Red"
    exit 1
}

Write-Status "Patching pilot files from controller config..."
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND", "--es", "command", "patch_pilots") | Out-Null
Start-Sleep -Seconds 2

Write-Status "Dumping launcher controller introspection..."
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND", "--es", "command", "controller_introspect") | Out-Null
Start-Sleep -Seconds 2

$launcherJson = Adb-Timeout -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/controller_introspect.json") -Seconds 5
if (-not $launcherJson -or $launcherJson.Length -lt 10) {
    Write-Status "FAIL: Could not read launcher controller introspection" "Red"
    exit 1
}
Write-Status "Launcher introspection read OK" "Green"

# ── Step 3: Push game script and launch game ─────────────────

if (-not (Send-AutomationScript $GAME_SCRIPT -PushOnly)) { exit 1 }

if (-not (Start-GameWithRetry)) {
    exit 1
}

# Send automation and wait for result
Start-Sleep -Seconds 2
Adb -AdbArgs @("logcat", "-c") | Out-Null
Write-Status "Sending automation broadcast..."
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", $GAME_SCRIPT) | Out-Null

$passed = Watch-AutomationResult -TimeoutSeconds $TimeoutSeconds
if (-not $passed) { exit 1 }
Write-Status "Game script PASS" "Green"

# ── Step 4: Read game introspection ──────────────────────────
Start-Sleep -Seconds 2
$gameJson = Adb-Timeout -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/introspect.json") -Seconds 5
if (-not $gameJson -or $gameJson.Length -lt 10) {
    Write-Status "FAIL: Could not read game introspection" "Red"
    exit 1
}
Write-Status "Game introspection read OK" "Green"

# ── Step 5: Compare joystick_controls ────────────────────────
Write-Status "Comparing joystick controls..."

$launcher = ($launcherJson | ConvertFrom-Json)
$game = ($gameJson | ConvertFrom-Json)

$launcherJC = $launcher.joystick_controls
$gameJC = $game.joystick_controls

# Compare summary fields
$summaryOk = $true
foreach ($field in @("control_type", "bound_count", "bound_controls", "total_count")) {
    $lv = $launcherJC.$field
    $gv = $gameJC.$field
    if ($lv -ne $gv) {
        Write-Host "  MISMATCH $field : launcher=$lv  game=$gv" -ForegroundColor Red
        $summaryOk = $false
    } else {
        Write-Host "  OK       $field : $lv" -ForegroundColor Green
    }
}

# Compare each item
$itemMismatches = 0
$launcherItems = $launcherJC.items
$gameItems = $gameJC.items
$count = [Math]::Min($launcherItems.Count, $gameItems.Count)

for ($i = 0; $i -lt $count; $i++) {
    $li = $launcherItems[$i]
    $gi = $gameItems[$i]
    $diffs = @()

    if ($li.name -cne $gi.name) { $diffs += "name: '$($li.name)' vs '$($gi.name)'" }
    if ($li.type -cne $gi.type) { $diffs += "type: '$($li.type)' vs '$($gi.type)'" }
    if ($li.value -ne $gi.value) { $diffs += "value: $($li.value) vs $($gi.value)" }

    if ($diffs.Count -gt 0) {
        $itemMismatches++
        $diffStr = $diffs -join ", "
        Write-Host "  MISMATCH item[$i] ($($gi.name)): $diffStr" -ForegroundColor Red
    } elseif ($li.value -ne 255) {
        Write-Host "  OK       item[$i] $($gi.name) = $($gi.value)" -ForegroundColor Green
    }
}

if ($launcherItems.Count -ne $gameItems.Count) {
    Write-Host "  MISMATCH item count: launcher=$($launcherItems.Count) game=$($gameItems.Count)" -ForegroundColor Red
    $itemMismatches++
}

# ── Result ───────────────────────────────────────────────────
Write-Host ""
if ($summaryOk -and $itemMismatches -eq 0) {
    Write-Status "PASS: Launcher and in-game joystick configs match perfectly" "Green"
    exit 0
} else {
    Write-Status "FAIL: $itemMismatches item mismatch(es) found" "Red"
    exit 1
}
