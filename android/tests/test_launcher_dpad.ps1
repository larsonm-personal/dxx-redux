# test_launcher_dpad.ps1 -- Verify D-pad/keyboard navigation in the launcher
# Usage: .\android\tests\test_launcher_dpad.ps1
# Requires: emulator running, app installed, game data available

param(
    [int]$TimeoutSeconds = 60,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\test_helpers.ps1"

function Fail($msg) { Write-Error "FAIL: $msg"; exit 1 }
function Info($msg) { Write-Host $msg }

function WaitMs($ms) {
    Start-Process -FilePath "cmd.exe" -ArgumentList "/c timeout $([Math]::Ceiling($ms / 1000))" -NoNewWindow -Wait
}

function GetSetupButtons {
    # Poll until setup_introspect.json has a non-empty buttons array.
    # Returns button text array, or fails after timeout.
    $result = Wait-SetupCondition -TimeoutSeconds 15 -PollMs 800 -Predicate {
        param($obj)
        return ($null -ne $obj.buttons -and $obj.buttons.Count -gt 0)
    }
    if (-not $result) {
        # Dump whatever we got for diagnostics
        $raw = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json") -Seconds 3
        Info "  DEBUG: setup_introspect.json content: $raw"
        Fail "Timed out waiting for setup buttons (15s)"
    }
    $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json") -Seconds 3
    $obj = $json | ConvertFrom-Json
    return $obj.buttons | ForEach-Object { $_.text }
}

function SendKey($code) {
    & $script:ADB shell input keyevent $code 2>$null
}

# --- Setup ---
Info "Checking emulator..."
Ensure-EmulatorHealthy

# Push game data so canLaunch=true and Multiplayer button is enabled/focusable
Info "Resolving game data deps..."
if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
    Fail "Could not resolve game data deps"
}

# Reset active file set to "default" -- previous tests may have switched sets
Info "Resetting file set state..."
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/file_sets.json") | Out-Null
Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/setup_introspect.json") | Out-Null

Info "Force-stopping app..."
& $script:ADB shell am force-stop com.dxxredux.app

Info "Launching SetupActivity..."
& $script:ADB shell am start -n "com.dxxredux.app/.SetupActivity" 2>&1 | Out-Null
Info "Waiting for SetupActivity to be ready..."
if (-not (Wait-SetupActivityReady -TimeoutSeconds 30)) {
    Fail "SetupActivity did not become ready within 30s"
}

# --- Test 1: Initial page shows main buttons ---
Info "Test 1: Verify main page buttons..."
$buttons = GetSetupButtons
Info "  Found buttons: $($buttons -join ', ')"
if ($buttons -notcontains "Multiplayer") { Fail "Main page missing Multiplayer button (got: $($buttons -join ', '))" }
Info "  PASS: Main page has expected buttons"

# --- Test 2: DPAD_CENTER activates Multiplayer (initial focus target) ---
Info "Test 2: DPAD_CENTER on initial focus (Multiplayer)..."
& $script:ADB logcat -c
SendKey 23  # DPAD_CENTER
WaitMs 2000
$buttons = GetSetupButtons
if ($buttons -contains "Multiplayer") {
    Fail "DPAD_CENTER did not navigate away from main page (focus not on Multiplayer)"
}
if ($buttons -notcontains "Back") {
    # Could show "< Back" or "Back" depending on the page
    $hasBack = $false
    foreach ($b in $buttons) { if ($b -match "Back") { $hasBack = $true; break } }
    if (-not $hasBack) { Fail "Expected a Back button on the sub-page" }
}
Info "  PASS: Navigated to sub-page via DPAD_CENTER"

# --- Test 3: BACK key returns to main page ---
Info "Test 3: BACK returns to main page..."
SendKey 4  # KEYCODE_BACK
WaitMs 2000
$buttons = GetSetupButtons
if ($buttons -notcontains "Multiplayer") { Fail "BACK did not return to main page (got: $($buttons -join ', '))" }
Info "  PASS: Returned to main page"

# --- Test 4: Focus restoration after return - DPAD_CENTER works again ---
Info "Test 4: Focus restoration after return..."
WaitMs 500
SendKey 23  # DPAD_CENTER
WaitMs 2000
$buttons = GetSetupButtons
if ($buttons -contains "Multiplayer") {
    Fail "Focus not restored after return from sub-page"
}
Info "  PASS: Focus restored, navigated to sub-page again"

# --- Test 5: DPAD navigation (UP from Multiplayer to a settings button) ---
Info "Test 5: DPAD UP navigation..."
SendKey 4  # BACK to main
WaitMs 2000
# UP 3x to skip past game selection chips and reach a settings button
SendKey 19  # DPAD_UP (to game chip)
SendKey 19  # DPAD_UP (to another chip or settings row)
SendKey 19  # DPAD_UP (to a settings button)
SendKey 23  # DPAD_CENTER
WaitMs 2000
$buttons = GetSetupButtons
if ($buttons -contains "Multiplayer") {
    Info "  DEBUG: buttons after UP-x3+CENTER: $($buttons -join ', ')"
    Fail "DPAD_UP + CENTER did not navigate (focus didn't move)"
}
Info "  PASS: DPAD_UP moved focus to a settings button"

# --- Cleanup ---
SendKey 4  # BACK
WaitMs 1000
& $script:ADB shell am force-stop com.dxxredux.app

Info ""
Info "All tests passed"
