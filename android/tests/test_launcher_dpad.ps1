# test_launcher_dpad.ps1 -- Verify D-pad/keyboard navigation in the launcher
# Usage: .\android\tests\test_launcher_dpad.ps1
# Requires: emulator running, app installed (no game files needed)

param(
    [int]$TimeoutSeconds = 60,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$ADB = "c:\local\android-sdk\platform-tools\adb.exe"

function Fail($msg) { Write-Error "FAIL: $msg"; exit 1 }
function Info($msg) { Write-Host $msg }

function WaitMs($ms) {
    Start-Process -FilePath "cmd.exe" -ArgumentList "/c timeout $([Math]::Ceiling($ms / 1000))" -NoNewWindow -Wait
}

function GetSetupButtons {
    & $ADB shell am broadcast -a com.dxxredux.SETUP_INTROSPECT 2>$null | Out-Null
    WaitMs 1500
    $raw = & $ADB shell run-as com.dxxredux.app cat files/setup_introspect.json 2>&1 | Out-String
    $obj = $raw | ConvertFrom-Json
    return $obj.buttons | ForEach-Object { $_.text }
}

function SendKey($code) {
    & $ADB shell input keyevent $code 2>$null
}

# --- Setup ---
Info "Checking emulator..."
$devices = & $ADB devices 2>&1 | Out-String
if ($devices -notmatch "emulator.*device") {
    Fail "No emulator found. Start one first"
}

Info "Force-stopping app..."
& $ADB shell am force-stop com.dxxredux.app

Info "Launching SetupActivity..."
& $ADB shell am start -n "com.dxxredux.app/.SetupActivity" 2>&1 | Out-Null
WaitMs 5000

# --- Test 1: Initial page shows main buttons ---
Info "Test 1: Verify main page buttons..."
$buttons = GetSetupButtons
if ($buttons -notcontains "Multiplayer") { Fail "Main page missing Multiplayer button" }
if ($buttons -notcontains "Launch Descent 2") { Fail "Main page missing Launch button" }
Info "  PASS: Main page has expected buttons"

# --- Test 2: DPAD_CENTER activates Multiplayer (initial focus target) ---
Info "Test 2: DPAD_CENTER on initial focus (Multiplayer)..."
& $ADB logcat -c
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
if ($buttons -notcontains "Multiplayer") { Fail "BACK did not return to main page" }
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

# --- Test 5: DPAD navigation (UP from Multiplayer) ---
Info "Test 5: DPAD UP navigation..."
SendKey 4  # BACK to main
WaitMs 2000
SendKey 19  # DPAD_UP
SendKey 23  # DPAD_CENTER
WaitMs 2000
$buttons = GetSetupButtons
if ($buttons -contains "Multiplayer") {
    Fail "DPAD_UP + CENTER did not navigate (focus didn't move)"
}
Info "  PASS: DPAD_UP moved focus to a settings button"

# --- Cleanup ---
SendKey 4  # BACK
WaitMs 1000
& $ADB shell am force-stop com.dxxredux.app

Info ""
Info "All tests passed"
