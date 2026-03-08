#!/usr/bin/env pwsh
# run_controller_compare.ps1 — Compare launcher controller config with in-game joystick bindings.
#
# Usage:
#   .\run_controller_compare.ps1
#   .\run_controller_compare.ps1 -Install   # install APK first
#
# Steps:
#   1. Launch SetupActivity
#   2. Patch pilots from controller_config.json
#   3. Dump launcher controller introspection
#   4. Launch game, load existing pilot, dump game introspection
#   5. Compare the two joystick_controls sections item by item

param(
    [switch]$Install,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Continue"
$ADB = "C:\local\android-sdk\platform-tools\adb.exe"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$GAME_SCRIPT = "test_controller_compare.json5"

function Write-Status {
    param([string]$Msg, [string]$Color = "Cyan")
    Write-Host "[$([DateTime]::Now.ToString('HH:mm:ss'))] $Msg" -ForegroundColor $Color
}

# ── Step 1: Install if requested ─────────────────────────────
if ($Install) {
    $apk = "$SCRIPT_DIR\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "FAIL: APK not found at $apk" "Red"
        exit 1
    }
    Write-Status "Installing APK..."
    & $ADB install -r $apk 2>&1 | Write-Host
}

# ── Step 2: Force stop and launch SetupActivity ─────────────
Write-Status "Force-stopping app..."
& $ADB shell am force-stop $PACKAGE 2>&1 | Out-Null
Start-Sleep -Seconds 2

Write-Status "Launching SetupActivity..."
& $ADB shell am start -n "$PACKAGE/$ACTIVITY" 2>&1 | Out-Null
Start-Sleep -Seconds 3

# ── Step 3: Patch pilots and dump launcher introspection ─────
Write-Status "Patching pilot files from controller config..."
& $ADB shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command patch_pilots 2>&1 | Out-Null
Start-Sleep -Seconds 2

Write-Status "Dumping launcher controller introspection..."
& $ADB shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command controller_introspect 2>&1 | Out-Null
Start-Sleep -Seconds 2

$launcherJson = & $ADB shell run-as $PACKAGE cat files/controller_introspect.json 2>&1 | Out-String
if (-not $launcherJson -or $launcherJson.Length -lt 10) {
    Write-Status "FAIL: Could not read launcher controller introspection" "Red"
    exit 1
}
Write-Status "Launcher introspection read OK" "Green"

# ── Step 4: Push game script and launch game ─────────────────
$scriptPath = "$SCRIPT_DIR\game_scripts\$GAME_SCRIPT"
if (-not (Test-Path $scriptPath)) {
    Write-Status "FAIL: Game script not found: $scriptPath" "Red"
    exit 1
}

Write-Status "Pushing game script: $GAME_SCRIPT"
& $ADB push $scriptPath /data/local/tmp/$GAME_SCRIPT 2>&1 | Out-Null
& $ADB shell run-as $PACKAGE cp /data/local/tmp/$GAME_SCRIPT files/$GAME_SCRIPT 2>&1 | Out-Null

Write-Status "Launching game..."
& $ADB shell am broadcast -a com.dxxredux.SETUP_COMMAND --es command launch 2>&1 | Out-Null
Start-Sleep -Seconds 8

# Clear logcat and send automation
& $ADB logcat -c 2>&1 | Out-Null
Write-Status "Sending automation broadcast..."
& $ADB shell am broadcast -a com.dxxredux.AUTOMATE --es script $GAME_SCRIPT 2>&1 | Out-Null

# ── Step 5: Wait for automation to complete ──────────────────
Write-Status "Waiting for game script (fixed 20s)..."
Start-Sleep -Seconds 20

$log = & $ADB logcat -d -s DXX-Automate 2>&1 | Out-String
Write-Host $log -ForegroundColor Gray

if ($log -match 'SCRIPT_RESULT:\s*PASS') {
    Write-Status "Game script PASS" "Green"
} elseif ($log -match 'SCRIPT_RESULT:\s*FAIL') {
    Write-Status "FAIL: Game script reported failure" "Red"
    exit 1
} else {
    Write-Status "FAIL: No SCRIPT_RESULT found in logcat" "Red"
    exit 1
}

# ── Step 6: Read game introspection ──────────────────────────
Start-Sleep -Seconds 2
$gameJson = & $ADB shell run-as $PACKAGE cat files/introspect.json 2>&1 | Out-String
if (-not $gameJson -or $gameJson.Length -lt 10) {
    Write-Status "FAIL: Could not read game introspection" "Red"
    exit 1
}
Write-Status "Game introspection read OK" "Green"

# ── Step 7: Compare joystick_controls ────────────────────────
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
