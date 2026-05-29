#!/usr/bin/env pwsh
# test_launcher_dpad.ps1 -- Verify D-pad/keyboard navigation in the launcher
# Usage: .\android\tests\test_launcher_dpad.ps1
# Requires: emulator running, app installed, game data available

param(
    [int]$TimeoutSeconds = 60,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

function Fail($msg) { Write-Error "FAIL: $msg"; exit 1 }
function Info($msg) { Write-Host $msg }

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

function GetSetupIntrospect {
    $result = Wait-SetupCondition -TimeoutSeconds 15 -PollMs 800 -Predicate {
        param($obj)
        return ($null -ne $obj.buttons -and $obj.buttons.Count -gt 0)
    }
    if (-not $result) {
        $raw = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json") -Seconds 3
        Info "  DEBUG: setup_introspect.json content: $raw"
        Fail "Timed out waiting for setup introspection (15s)"
    }

    $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_introspect.json") -Seconds 3
    return $json | ConvertFrom-Json
}

function Wait-ForMainPageDpadReady {
    param(
        [int]$TimeoutSeconds = 8,
        [int]$SettleMs = 1500
    )

    $ready = Wait-SetupCondition -TimeoutSeconds $TimeoutSeconds -PollMs 400 -Predicate {
        param($obj)
        if ($null -eq $obj.buttons -or $obj.buttons.Count -eq 0) {
            return $false
        }
        $texts = @($obj.buttons | ForEach-Object { $_.text })
        return (($texts -contains "Define Controls") -and ($texts -contains "Touch Layout"))
    }

    if (-not $ready) {
        $buttons = GetSetupButtons
        Fail "Main page did not become ready for D-pad navigation (got: $($buttons -join ', '))"
    }

    Start-Sleep -Milliseconds $SettleMs

    return GetSetupButtons
}

function Test-ContainsButton {
    param(
        [string[]]$Buttons,
        [string]$Text
    )

    return $Buttons -contains $Text
}

function Test-ContainsButtonMatch {
    param(
        [string[]]$Buttons,
        [string]$Pattern
    )

    foreach ($button in $Buttons) {
        if ($button -match $Pattern) {
            return $true
        }
    }
    return $false
}

function Wait-ForSetupButtons {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Predicate,
        [Parameter(Mandatory = $true)][string]$Failure,
        [int]$TimeoutSeconds = 10
    )

    $matched = Wait-SetupCondition -TimeoutSeconds $TimeoutSeconds -PollMs 500 -Predicate {
        param($obj)
        if ($null -eq $obj.buttons -or $obj.buttons.Count -eq 0) {
            return $false
        }
        $texts = @($obj.buttons | ForEach-Object { $_.text })
        return (& $Predicate (, $texts))
    }

    $buttons = GetSetupButtons
    if (-not $matched) {
        Fail "$Failure (got: $($buttons -join ', '))"
    }
    return $buttons
}

function Wait-ForSetupButtonFocus {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [int]$TimeoutSeconds = 10
    )

    $matched = Wait-SetupCondition -TimeoutSeconds $TimeoutSeconds -PollMs 500 -Predicate {
        param($obj)
        if ($null -eq $obj.buttons -or $obj.buttons.Count -eq 0) {
            return $false
        }

        foreach ($button in $obj.buttons) {
            if ($button.text -eq $Text -and $button.focused) {
                return $true
            }
        }

        return $false
    }

    $obj = GetSetupIntrospect
    $focused = @($obj.buttons | Where-Object { $_.focused } | ForEach-Object { $_.text })
    if (-not $matched) {
        Fail "Timed out waiting for focus on '$Text' (focused: $($focused -join ', '))"
    }
}

function SendKey($code) {
    & $script:ADB shell input keyevent $code 2>$null
}

function EnableLauncherDpadMode {
    SendKey 103  # BUTTON_R1 enters controller-navigation mode without moving focus
}

function SendSetupCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [string[]]$Extras = @()
    )

    $broadcastCommand = @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
        "--es", "command", $Command
    ) + $Extras
    Adb -AdbArgs $broadcastCommand | Out-Null
}

# --- Setup ---
Info "Checking emulator..."
Ensure-EmulatorHealthy

# Push game data so launcher pages that depend on game-ready state are available.
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
& $script:ADB shell am force-stop com.google.android.documentsui 2>$null

Info "Launching SetupActivity..."
& $script:ADB shell am start -n "com.dxxredux.app/.SetupActivity" 2>&1 | Out-Null
Info "Waiting for SetupActivity to be ready..."
if (-not (Wait-SetupActivityReady -TimeoutSeconds 30)) {
    Fail "SetupActivity did not become ready within 30s"
}

Info "Clearing save and resume-offer state..."
SendSetupCommand -Command "clear_save_files"
SendSetupCommand -Command "write_bool_pref" -Extras @("--es", "key", "show_resume_offer", "--ez", "value", "false")
Start-Sleep -Milliseconds 500

# --- Test 1: Initial page shows current launcher controls ---
Info "Test 1: Verify main page buttons..."
$buttons = Wait-ForMainPageDpadReady -TimeoutSeconds 15 -SettleMs 500
Info "  Found buttons: $($buttons -join ', ')"
if (-not (Test-ContainsButton -Buttons $buttons -Text "Define Controls")) {
    Fail "Main page missing Define Controls button (got: $($buttons -join ', '))"
}
if (-not (Test-ContainsButton -Buttons $buttons -Text "Touch Layout")) {
    Fail "Main page missing Touch Layout button (got: $($buttons -join ', '))"
}
Info "  PASS: Main page has expected buttons"

# --- Test 2: DPAD_CENTER activates Define Controls (initial focus target) ---
Info "Test 2: DPAD_CENTER on initial focus (Define Controls)..."
$buttons = Wait-ForMainPageDpadReady
EnableLauncherDpadMode
Wait-ForSetupButtonFocus -Text "Define Controls"
& $script:ADB logcat -c
SendKey 23  # DPAD_CENTER
$buttons = GetSetupButtons
if (($buttons -contains "Define Controls") -or
    ($buttons -notcontains "Cancel") -or
    (@($buttons | Where-Object { $_ -like "Save*" }).Count -eq 0)) {
    Fail "DPAD_CENTER did not navigate to the controller page (got: $($buttons -join ', '))"
}
Info "  PASS: Navigated to sub-page via DPAD_CENTER"

# --- Test 3: BACK key returns to main page ---
Info "Test 3: BACK returns to main page..."
SendKey 4  # KEYCODE_BACK
$buttons = Wait-ForMainPageDpadReady
if (($buttons -notcontains "Define Controls") -or ($buttons -notcontains "Touch Layout")) {
    Fail "BACK did not return to the main page (got: $($buttons -join ', '))"
}
Info "  PASS: Returned to main page"

# --- Test 4: Focus restoration after return - DPAD_CENTER works again ---
Info "Test 4: Focus restoration after return..."
Wait-ForSetupButtonFocus -Text "Define Controls"
SendKey 23  # DPAD_CENTER
$buttons = GetSetupButtons
if (($buttons -contains "Define Controls") -or
    ($buttons -notcontains "Cancel") -or
    (@($buttons | Where-Object { $_ -like "Save*" }).Count -eq 0)) {
    Fail "Focus was not restored after returning from the controller page (got: $($buttons -join ', '))"
}
Info "  PASS: Focus restored, navigated to sub-page again"

# --- Test 5: DPAD_RIGHT moves from Define Controls to Touch Layout ---
Info "Test 5: DPAD RIGHT navigation..."
SendKey 4  # BACK to main
$buttons = Wait-ForMainPageDpadReady
if (($buttons -notcontains "Define Controls") -or ($buttons -notcontains "Touch Layout")) {
    Fail "Did not return to the main page before DPAD_RIGHT test (got: $($buttons -join ', '))"
}
Wait-ForSetupButtonFocus -Text "Define Controls"
SendKey 22  # DPAD_RIGHT to Touch Layout
SendKey 23  # DPAD_CENTER
$buttons = GetSetupButtons
if ($buttons -notcontains "Close Editor") {
    Info "  DEBUG: buttons after RIGHT+CENTER: $($buttons -join ', ')"
    Fail "DPAD_RIGHT + CENTER did not open the Touch Layout editor"
}
Info "  PASS: DPAD_RIGHT moved focus to Touch Layout"

# --- Cleanup ---
SendKey 4  # BACK
& $script:ADB shell am force-stop com.dxxredux.app

Info ""
Info "All tests passed"
