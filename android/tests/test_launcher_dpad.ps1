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
Initialize-AndroidTestTarget | Out-Null
Assert-IsolatedPhysicalTestApp

function Fail($msg) { Write-Error "FAIL: $msg"; exit 1 }
function Info($msg) { Write-Host $msg }

function GetSetupButtons {
    # Poll until setup_buttons.json has a non-empty buttons array.
    # Returns button text array, or fails after timeout.
    $result = Wait-SetupCondition -ButtonsOnly -TimeoutSeconds 15 -PollMs 800 -Predicate {
        param($obj)
        return ($null -ne $obj.buttons -and $obj.buttons.Count -gt 0)
    }
    if (-not $result) {
        # Dump whatever we got for diagnostics
        $raw = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_buttons.json") -Seconds 3
        Info "  DEBUG: setup_buttons.json content: $raw"
        Fail "Timed out waiting for setup buttons (15s)"
    }
    $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_buttons.json") -Seconds 3
    $obj = $json | ConvertFrom-Json
    return $obj.buttons | ForEach-Object { $_.text }
}

function GetSetupIntrospect {
    $result = Wait-SetupCondition -ButtonsOnly -TimeoutSeconds 15 -PollMs 800 -Predicate {
        param($obj)
        return ($null -ne $obj.buttons -and $obj.buttons.Count -gt 0)
    }
    if (-not $result) {
        $raw = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_buttons.json") -Seconds 3
        Info "  DEBUG: setup_buttons.json content: $raw"
        Fail "Timed out waiting for setup introspection (15s)"
    }

    $json = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "files/setup_buttons.json") -Seconds 3
    return $json | ConvertFrom-Json
}

function Wait-ForMainPageDpadReady {
    param(
        [int]$TimeoutSeconds = 8,
        [int]$SettleMs = 1500
    )

    $ready = Wait-SetupCondition -ButtonsOnly -TimeoutSeconds $TimeoutSeconds -PollMs 400 -Predicate {
        param($obj)
        if ($null -eq $obj.buttons -or $obj.buttons.Count -eq 0) {
            return $false
        }
        $texts = @($obj.buttons | ForEach-Object { $_.text })
        return (($texts -contains "Settings") -and ($texts -contains "Multiplayer") -and ($texts -contains "Import files"))
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

    $buttonPredicate = $Predicate
    $matched = Wait-SetupCondition -ButtonsOnly -TimeoutSeconds $TimeoutSeconds -PollMs 500 -Predicate {
        param($obj)
        if ($null -eq $obj.buttons -or $obj.buttons.Count -eq 0) {
            return $false
        }
        $texts = @($obj.buttons | ForEach-Object { $_.text })
        return (& $buttonPredicate $texts)
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

    $matched = Wait-SetupCondition -ButtonsOnly -TimeoutSeconds $TimeoutSeconds -PollMs 500 -Predicate {
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
    $result = Adb-Timeout -AdbArgs @("shell", "input", "keyevent", [string]$code) -Seconds 5
    if ($null -eq $result) {
        Fail "ADB timed out sending keyevent $code"
    }
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
    $result = Adb-Timeout -AdbArgs $broadcastCommand -Seconds 5
    if ($null -eq $result) {
        Fail "ADB timed out sending setup command '$Command'"
    }
}

$script:launcherDpadCleanupDone = $false
function Invoke-LauncherDpadCleanup {
    if ($script:launcherDpadCleanupDone) { return }
    $script:launcherDpadCleanupDone = $true
    try { Stop-AppAndWait } catch {}
}
Register-EngineEvent PowerShell.Exiting -Action { Invoke-LauncherDpadCleanup } | Out-Null

# --- Setup ---
Info "Checking emulator..."
Ensure-EmulatorHealthy

# Push game data so launcher pages that depend on game-ready state are available.
Info "Resolving game data deps..."
if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
    Fail "Could not resolve game data deps"
}

Info "Force-stopping app..."
Stop-AppAndWait
Reset-GameState
& $script:ADB shell am force-stop com.google.android.documentsui 2>$null

Info "Launching SetupActivity..."
Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
Info "Waiting for SetupActivity to be ready..."
if (-not (Wait-SetupActivityReady -TimeoutSeconds 30)) {
    Fail "SetupActivity did not become ready within 30s"
}

Info "Clearing save and resume-offer state..."
SendSetupCommand -Command "clear_save_files"
SendSetupCommand -Command "write_bool_pref" -Extras @("--es", "key", "show_resume_offer", "--ez", "value", "false")
Start-Sleep -Milliseconds 500

function Tap-SetupButton {
    param([string]$Text)
    Adb-Timeout -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.MP_COMMAND", "--es", "command", "tap_button", "--es", "text", $Text) -Seconds 8 | Out-Null
}

function Get-MusicMode {
    $xml = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "cat", "shared_prefs/dxx_prefs.xml") -Seconds 5
    $match = [regex]::Match(($xml -join "`n"), '<string name="music_mode">([^<]*)</string>')
    return $match.Groups[1].Value
}

Info "Test 1: Home is focused on playing and importing, with one Settings gear"
EnableLauncherDpadMode
$buttons = Wait-ForMainPageDpadReady -TimeoutSeconds 15 -SettleMs 500
foreach ($oldShortcut in @("Controller Mapping", "Touch Layout", "Weapon Autoselect", "Game Preferences", "Graphics", "Advanced", "About")) {
    if ($buttons -contains $oldShortcut) { Fail "Old settings shortcut remains on Home: $oldShortcut" }
}
if (@($buttons | Where-Object { $_ -eq "Settings" }).Count -ne 1) { Fail "Home must have exactly one Settings entry" }
if (-not (Test-ContainsButtonMatch -Buttons $buttons -Pattern '^Launch Descent')) { Fail "Start game choices missing" }

Info "Test 2: Home > Settings > Controller > mapping, then restore focus"
Tap-SetupButton "Settings"
$null = Wait-ForSetupButtons -Failure "Settings menu did not open" -Predicate { param($texts) $texts -contains "Controller" }
Wait-ForSetupButtonFocus -Text "Controller"
SendKey 23
$null = Wait-ForSetupButtons -Failure "Controller submenu did not open" -Predicate { param($texts) $texts -contains "Controller Mapping" }
Wait-ForSetupButtonFocus -Text "Controller Mapping"
SendKey 23
$null = Wait-ForSetupButtons -Failure "Mapping editor did not open" -Predicate { param($texts) ($texts -contains "Cancel") -and (@($texts | Where-Object { $_ -like "Save*" }).Count -gt 0) }
SendKey 4
$null = Wait-ForSetupButtons -Failure "Mapping Back did not return to Controller" -Predicate { param($texts) $texts -contains "Controller Mapping" }
Wait-ForSetupButtonFocus -Text "Controller Mapping"
SendKey 4
$null = Wait-ForSetupButtons -Failure "Controller Back did not return to Settings" -Predicate { param($texts) $texts -contains "Controller" }
Wait-ForSetupButtonFocus -Text "Controller"

Info "Test 3: Search opens Music passively and returns to the same result"
$musicBefore = Get-MusicMode
SendKey 19 # Up from first menu item reaches search
SendKey 23 # Activate search without auto-opening it on entry
Adb-Timeout -AdbArgs @("shell", "input", "text", "redbook") -Seconds 5 | Out-Null
$null = Wait-ForSetupButtons -Failure "Search did not find CD Audio" -Predicate { param($texts) @($texts | Where-Object { $_ -like "CD Audio*" }).Count -gt 0 }
SendKey 4 # IME consumes Back before page navigation
SendKey 20
SendKey 23
$null = Wait-ForSetupButtons -Failure "Search result did not open Music" -Predicate { param($texts) ($texts -contains "MIDI") -and ($texts -contains "CD Audio") }
if ((Get-MusicMode) -ne $musicBefore) { Fail "Opening a music search result changed music_mode" }
SendKey 4
$buttons = Wait-ForSetupButtons -Failure "Music Back did not restore search results" -Predicate { param($texts) ($texts -contains "Clear") -and (@($texts | Where-Object { $_ -like "CD Audio*" }).Count -gt 0) }
$resultLabel = $buttons | Where-Object { $_ -like "CD Audio*" } | Select-Object -First 1
Wait-ForSetupButtonFocus -Text $resultLabel
Tap-SetupButton "Clear"

Info "Test 4: Touch submenu and full-screen editor return one level"
Tap-SetupButton "Touch controls"
$null = Wait-ForSetupButtons -Failure "Touch submenu did not open" -Predicate { param($texts) $texts -contains "Edit touch layout" }
Wait-ForSetupButtonFocus -Text "Edit touch layout"
SendKey 23
$null = Wait-ForSetupButtons -Failure "Touch editor did not open" -Predicate { param($texts) $texts -contains "Close Editor" }
SendKey 4
$null = Wait-ForSetupButtons -Failure "Touch editor Back did not restore its submenu" -Predicate { param($texts) $texts -contains "Edit touch layout" }
Wait-ForSetupButtonFocus -Text "Edit touch layout"
SendKey 4
$null = Wait-ForSetupButtons -Failure "Touch Back did not return to Settings" -Predicate { param($texts) $texts -contains "Controller" }
SendKey 4
$null = Wait-ForMainPageDpadReady
Wait-ForSetupButtonFocus -Text "Settings"

Info "Test 5: Reopening Settings begins a new visit without the previous search"
SendKey 23
$buttons = Wait-ForSetupButtons -Failure "Settings did not reopen" -Predicate { param($texts) $texts -contains "Controller" }
if ($buttons -contains "Clear") { Fail "A new Settings visit retained the old query" }
SendKey 4

Info "All launcher D-pad tests passed"
Invoke-LauncherDpadCleanup
