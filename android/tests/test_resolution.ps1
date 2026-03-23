#!/usr/bin/env pwsh
# test_resolution.ps1 -- Two-phase resolution verification test.
#
# Phase 1: Fresh launch (no descent.cfg) -> verifies SetupActivity writes default
#           1/2-screen resolution and the game uses it.
# Phase 2: Write descent.cfg with 640x480 -> verifies the game applies that resolution.
#
# Usage:
#   .\test_resolution.ps1
#   .\test_resolution.ps1 -Install   # install APK first
#
# Expects emulator to be running. For Pixel 6 (2400x1080 landscape):
#   1/2 screen = 1200x540

param(
    [switch]$Install
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\test_helpers.ps1"

$ScriptName = "test_resolution.json5"

# -- Helpers --------------------------------------------------

function Get-ResolutionFromIntrospection {
    $intro = Get-GameIntrospection
    if (-not $intro -or -not $intro.resolution) {
        Write-Status "FAIL: Could not read resolution from introspection" "Red"
        return $null
    }
    return $intro.resolution
}

# -- Pre-flight -----------------------------------------------

Ensure-EmulatorHealthy

# Resolve game data deps from script
$deps = Get-ScriptDeps -ScriptPath $scriptPath
if ($deps) {
    if (-not (Resolve-GameDataDeps -Deps $deps)) {
        Write-Status "FAIL: Could not resolve game data deps" "Red"
        exit 1
    }
} else {
    if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
        Write-Status "FAIL: Could not resolve game data deps" "Red"
        exit 1
    }
}

if ($Install) {
    $apk = "$PSScriptRoot\..\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "FAIL: APK not found at $apk" "Red"
        exit 1
    }
    Write-Status "Installing APK..."
    Adb -AdbArgs @("install", "-r", $apk) | Write-Host
}

# Resolve and push test script to device
$scriptPath = Join-Path "$PSScriptRoot\..\game_scripts" $ScriptName
if (-not (Test-Path $scriptPath)) {
    Write-Status "FAIL: Script not found: $scriptPath" "Red"
    exit 1
}
$resolvedPath = Resolve-TestScript -ScriptPath $scriptPath -GameId "d2"
$pushSrc = if ($resolvedPath -ne $scriptPath) { $resolvedPath } else { $scriptPath }
Adb -AdbArgs @("push", $pushSrc, "/data/local/tmp/$ScriptName") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp",
    "/data/local/tmp/$ScriptName", "files/$ScriptName") | Out-Null

# ================================================================
#  PHASE 1: Default resolution (fresh launch, no descent.cfg)
# ================================================================
Write-Host ""
Write-Host "============================================================" -ForegroundColor White
Write-Host "  PHASE 1: Default half-screen resolution" -ForegroundColor White
Write-Host "============================================================" -ForegroundColor White

$preLaunch = {
    Reset-GameState
}

if (-not (Start-GameWithRetry -PreLaunchScript $preLaunch -Game "d2" -SkipGameData)) {
    exit 1
}

# Run automation
Start-Sleep -Seconds 2
Adb -AdbArgs @("logcat", "-c") | Out-Null
Write-Status "Sending automation broadcast for: $ScriptName"
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE",
    "--es", "script", $ScriptName) | Out-Null

$passed1 = Watch-AutomationResult -TimeoutSeconds 60
if (-not $passed1) {
    Write-Status "FAIL: Phase 1 automation did not pass" "Red"
    exit 1
}

# Check resolution introspection
$res1 = Get-ResolutionFromIntrospection
if (-not $res1) { exit 1 }

Write-Status ("Phase 1 resolution: render={0}x{1}  display={2}x{3}" -f `
        $res1.render_width, $res1.render_height, `
        $res1.display_width, $res1.display_height)

# Verify render resolution is NOT 640x480 (should be half-screen)
if ($res1.render_width -eq 640 -and $res1.render_height -eq 480) {
    Write-Status "FAIL: Phase 1 got 640x480 -- default half-screen resolution not applied" "Red"
    exit 1
}
# Verify render resolution is roughly half the display (within 10% tolerance)
$expectedW = [int]($res1.display_width / 2)
$expectedH = [int]($res1.display_height / 2)
if ($res1.render_width -lt ($expectedW * 0.9) -or $res1.render_height -lt ($expectedH * 0.9)) {
    Write-Status ("FAIL: Phase 1 render resolution too small: {0}x{1} (expected ~{2}x{3})" -f `
            $res1.render_width, $res1.render_height, $expectedW, $expectedH) "Red"
    exit 1
}
Write-Status "Phase 1 PASS: Default resolution is $($res1.render_width)x$($res1.render_height)" "Green"

# ================================================================
#  PHASE 2: Explicit 640x480 via descent.cfg
# ================================================================
Write-Host ""
Write-Host "============================================================" -ForegroundColor White
Write-Host "  PHASE 2: Explicit 640x480 resolution" -ForegroundColor White
Write-Host "============================================================" -ForegroundColor White

$preLaunch2 = {
    Reset-GameState

    # Write descent.cfg with 640x480 -- SetupActivity's writeInitialGameConfig
    # only writes when the file is missing, so pre-creating it pins the resolution.
    # Must include AspectX/Y: game uses these for sc_aspect calculation in gr_set_mode.
    # 640x480 is 4:3, so AspectX=3 (height component) AspectY=4 (width component).
    Write-Status "Writing descent.cfg with 640x480 (4:3 aspect)"
    $cfgContent = "AspectX=3`nAspectY=4`nResolutionX=640`nResolutionY=480`n"
    # Write to /data/local/tmp then copy with run-as
    $cfgContent | & $script:ADB shell "run-as $($script:PACKAGE) tee files/descent.cfg" | Out-Null
}

if (-not (Start-GameWithRetry -PreLaunchScript $preLaunch2 -Game "d2" -SkipGameData)) {
    exit 1
}

# Re-push resolved script (may have been cleaned)
Adb -AdbArgs @("push", $pushSrc, "/data/local/tmp/$ScriptName") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp",
    "/data/local/tmp/$ScriptName", "files/$ScriptName") | Out-Null

# Run automation
Start-Sleep -Seconds 2
Adb -AdbArgs @("logcat", "-c") | Out-Null
Write-Status "Sending automation broadcast for: $ScriptName"
Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE",
    "--es", "script", $ScriptName) | Out-Null

$passed2 = Watch-AutomationResult -TimeoutSeconds 60
if (-not $passed2) {
    Write-Status "FAIL: Phase 2 automation did not pass" "Red"
    exit 1
}

$res2 = Get-ResolutionFromIntrospection
if (-not $res2) { exit 1 }

Write-Status ("Phase 2 resolution: render={0}x{1}  display={2}x{3}" -f `
        $res2.render_width, $res2.render_height, `
        $res2.display_width, $res2.display_height)

# Verify render resolution IS 640x480
if ($res2.render_width -ne 640 -or $res2.render_height -ne 480) {
    Write-Status ("FAIL: Phase 2 expected 640x480, got {0}x{1}" -f `
            $res2.render_width, $res2.render_height) "Red"
    exit 1
}
Write-Status "Phase 2 PASS: Resolution correctly set to 640x480" "Green"

# ================================================================
#  Summary
# ================================================================
Write-Host ""
Write-Host "============================================================" -ForegroundColor Green
Write-Host "  ALL PHASES PASSED" -ForegroundColor Green
Write-Host "    Phase 1: Default = $($res1.render_width)x$($res1.render_height)" -ForegroundColor Green
Write-Host "    Phase 2: Explicit = $($res2.render_width)x$($res2.render_height)" -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Green
exit 0
