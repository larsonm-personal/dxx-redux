#!/usr/bin/env pwsh
# test_controller_compare.ps1 -- Compare launcher controller config with in-game joystick bindings.
#
# Usage:
#   .\test_controller_compare.ps1
#   .\test_controller_compare.ps1 -Install   # install APK first
#
# Steps:
#   1. Health check (restart emulator if needed)
#   2. Launch SetupActivity, patch pilots, dump launcher controller introspection
#   3. Launch game, run automation script, dump game introspection
#   4. Compare the two joystick_controls sections item by item

param(
    [switch]$Install,
    [int]$TimeoutSeconds = 120,
    [ValidateSet("d1","d2")]
    [string]$Game
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\..\test_helpers.ps1"

$GAME_SCRIPT = "test_controller_compare.json5"

# -- Determine game(s) to run --------------------------------

$scriptPath = Join-Path "$PSScriptRoot\..\game_scripts" $GAME_SCRIPT
$gameList = Get-ScriptGameInfo -ScriptPath $scriptPath
if ($Game) {
    $gameList = @($Game)
} elseif (-not $gameList) {
    $gameList = @("d2")
}

if ($gameList.Count -gt 1) {
    Write-Status "Multi-game script -- will run for: $($gameList -join ', ')"
}

# -- Step 1: Health check + install ---------------------------

Ensure-EmulatorHealthy

# Resolve game data deps up front
if (-not (Resolve-GameDataDeps -Deps (Get-StandardGameDataDeps))) {
    Write-Status "FAIL: Could not resolve game data deps" "Red"
    exit 1
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

$allPassed = $true
foreach ($gameId in $gameList) {
    if ($gameList.Count -gt 1) {
        Write-Host ""
        Write-Host "============================================================" -ForegroundColor White
        Write-Host "  Controller Compare: $($gameId.ToUpper())" -ForegroundColor White
        Write-Host "============================================================" -ForegroundColor White
    }

    $extraArgs = @()
    if ($gameId -eq "d1") { $extraArgs = @("--es", "game", "d1") }

    # -- Step 2: Launch SetupActivity and do pre-game introspection --

    Write-Status "Force-stopping app..."
    Adb -AdbArgs @("shell", "am", "force-stop", $PACKAGE) | Out-Null
    Start-Sleep -Seconds 2

    Adb -AdbArgs @("logcat", "-c") | Out-Null
    Write-Status "Launching SetupActivity..."
    Adb -AdbArgs @("shell", "am", "start", "-n", "$PACKAGE/$ACTIVITY") | Out-Null

    if (-not (Wait-SetupActivityReady)) {
        Write-Status "FAIL: SetupActivity not responding" "Red"
        $allPassed = $false; continue
    }

    Write-Status "Patching pilot files from controller config..."
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND", "--es", "command", "patch_pilots") | Out-Null
    Start-Sleep -Seconds 2

    Write-Status "Dumping launcher controller introspection for $($gameId.ToUpper())..."
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
        "--es", "command", "controller_introspect", "--es", "game", $gameId) | Out-Null
    Start-Sleep -Seconds 2

    $launcherJson = Adb-Timeout -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/controller_introspect.json") -Seconds 5
    if (-not $launcherJson -or $launcherJson.Length -lt 10) {
        Write-Status "FAIL: Could not read launcher controller introspection" "Red"
        $allPassed = $false; continue
    }
    Write-Status "Launcher introspection read OK" "Green"

    # -- Step 3: Resolve + push game script and launch game ---------

    $resolvedPath = Resolve-TestScript -ScriptPath $scriptPath -GameId $gameId
    $pushSrc = if ($resolvedPath -ne $scriptPath) { $resolvedPath } else { $scriptPath }
    Write-Status "Pushing resolved test script: $GAME_SCRIPT"
    Adb -AdbArgs @("push", $pushSrc, "/data/local/tmp/$GAME_SCRIPT") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $PACKAGE, "cp", "/data/local/tmp/$GAME_SCRIPT", "files/$GAME_SCRIPT") | Out-Null

    if (-not (Start-GameWithRetry -ExtraLaunchArgs $extraArgs -Game $gameId -SkipGameData)) {
        $allPassed = $false; continue
    }

    # Clear stale results and send automation
    Start-Sleep -Seconds 2
    Adb -AdbArgs @("logcat", "-c") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $PACKAGE, "rm", "-f", "files/automation_log.jsonl") | Out-Null
    Write-Status "Sending automation broadcast..."
    Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", $GAME_SCRIPT) | Out-Null

    $passed = Watch-AutomationResult -TimeoutSeconds $TimeoutSeconds
    if (-not $passed) { $allPassed = $false; continue }
    Write-Status "Game script PASS" "Green"

    # -- Step 4: Read game introspection --------------------------
    Start-Sleep -Seconds 2
    $gameJson = Adb-Timeout -AdbArgs @("shell", "run-as", $PACKAGE, "cat", "files/introspect.json") -Seconds 5
    if (-not $gameJson -or $gameJson.Length -lt 10) {
        Write-Status "FAIL: Could not read game introspection" "Red"
        $allPassed = $false; continue
    }
    Write-Status "Game introspection read OK" "Green"

    # -- Step 5: Compare joystick_controls ------------------------
    Write-Status "Comparing joystick controls..."

    $launcherIntro = ($launcherJson | ConvertFrom-Json)
    $gameIntro = ($gameJson | ConvertFrom-Json)

    $launcherJC = $launcherIntro.joystick_controls
    $gameJC = $gameIntro.joystick_controls

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

    # -- Result for this game -------------------------------------
    Write-Host ""
    if ($summaryOk -and $itemMismatches -eq 0) {
        Write-Status "PASS: Launcher and in-game joystick configs match ($($gameId.ToUpper()))" "Green"
    } else {
        Write-Status "FAIL: $itemMismatches item mismatch(es) found ($($gameId.ToUpper()))" "Red"
        $allPassed = $false
    }
}

if ($allPassed) { exit 0 } else { exit 1 }
