#!/usr/bin/env pwsh
# run_test.ps1 -- Run an automation test script on the Android emulator with health checks.
#
# Usage:
#   .\run_test.ps1 test_death.json5
#   .\run_test.ps1 test_death.json5 -Install   # also install APK first
#   .\run_test.ps1 test_axis_mapping.json5 -Game d1  # force a specific game
#
# The script will:
#   1. Verify emulator is healthy (restart if needed)
#   2. Push the test script to the device
#   3. Launch the app (SetupActivity -> game)
#   4. Wait for the game to start, then send the automation broadcast
#   5. Tail logcat for DXX-Automate, with periodic health checks
#   6. Report PASS/FAIL and exit

param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$ScriptName,
    [switch]$Install,
    [int]$TimeoutSeconds = 300,
    [ValidateSet("d1", "d2")]
    [string]$Game,
    [hashtable]$Params = @{}
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\test_helpers.ps1"

# -- Step 1: Health check -------------------------------------

Ensure-EmulatorHealthy

# -- Step 2: Install APK if requested ------------------------

if ($Install) {
    $apk = "$PSScriptRoot\app\build\outputs\apk\debug\app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "FAIL: APK not found at $apk" "Red"
        exit 1
    }
    Write-Status "Installing APK..."
    Adb -AdbArgs @("install", "-r", $apk) | Write-Host
}

# -- Step 3: Determine which game(s) to run -------------------

$scriptPath = Join-Path "$PSScriptRoot\game_scripts" $ScriptName
$gameList = Get-ScriptGameInfo -ScriptPath $scriptPath

if (-not (Get-ScriptStandalone -ScriptPath $scriptPath)) {
    Write-Status "WARNING: $ScriptName is a support script (not standalone). It is normally run by another test" "Yellow"
}

if ($Game) {
    # Explicit -Game parameter overrides _info
    $gameList = @($Game)
} elseif (-not $gameList) {
    # Fallback: legacy _d1_ filename heuristic
    if ($ScriptName -match '_d1_') {
        $gameList = @("d1")
    } else {
        $gameList = @("d2")
    }
}

if ($gameList.Count -gt 1) {
    Write-Status "Multi-game script -- will run for: $($gameList -join ', ')"
}

# -- Run for each game ---------------------------------------

$allPassed = $true
foreach ($gameId in $gameList) {
    if ($gameList.Count -gt 1) {
        Write-Host ""
        Write-Host "============================================================" -ForegroundColor White
        Write-Host "  Running as $($gameId.ToUpper())" -ForegroundColor White
        Write-Host "============================================================" -ForegroundColor White
    }

    # -- Resolve params: prompt for missing required params --------

    $scriptParams = Get-ScriptParams -ScriptPath $scriptPath
    $resolvedParams = @{} + $Params
    if ($scriptParams) {
        foreach ($prop in $scriptParams.PSObject.Properties) {
            $pName = $prop.Name
            if (-not $resolvedParams.ContainsKey($pName)) {
                $pDef = $prop.Value
                $optKeys = @($pDef.options.PSObject.Properties.Name)
                $label = if ($pDef.label) { $pDef.label } else { $pName }
                Write-Host ""
                Write-Host "$label -- select a value:" -ForegroundColor White
                for ($pi = 0; $pi -lt $optKeys.Count; $pi++) {
                    Write-Host "  $($pi + 1)) $($optKeys[$pi])"
                }
                $pc = Read-Host "Select (1-$($optKeys.Count))"
                $pci = 0
                if ([int]::TryParse($pc, [ref]$pci) -and $pci -ge 1 -and $pci -le $optKeys.Count) {
                    $resolvedParams[$pName] = $optKeys[$pci - 1]
                } else {
                    Write-Status "Invalid selection, defaulting to $($optKeys[0])" "Yellow"
                    $resolvedParams[$pName] = $optKeys[0]
                }
            }
        }
    }

    # -- Resolve script (variable substitution + conditional filtering) --

    $resolvedPath = Resolve-TestScript -ScriptPath $scriptPath -GameId $gameId -Params $resolvedParams
    $pushName = $ScriptName
    if ($resolvedPath -ne $scriptPath) {
        # Push the resolved file instead, but keep the original name on device
        $pushSrc = $resolvedPath
    } else {
        $pushSrc = $scriptPath
    }
    Write-Status "Pushing test script: $ScriptName"
    Adb -AdbArgs @("push", $pushSrc, "/data/local/tmp/$pushName") | Out-Null
    Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/$pushName", "files/$pushName") | Out-Null

    # -- Resolve declarative game data deps (if present) ----------

    # Build vars for dep substitution (game vars + param vars)
    $depVars = @{} + $resolvedParams
    $skipGameData = $false
    $deps = Get-ScriptDeps -ScriptPath $scriptPath -Vars $depVars
    if ($deps) {
        Write-Status "Resolving $($deps.Count) declared game data deps..."
        if (-not (Resolve-GameDataDeps -Deps $deps)) {
            $allPassed = $false
            if ($gameList.Count -gt 1) { Write-Status "FAIL for $($gameId.ToUpper())" "Red"; continue }
            exit 1
        }
        $skipGameData = $true
    }

    # -- Detect launcher vs game script ---------------------------

    $isLauncherScript = Get-ScriptIsLauncher -ScriptPath $scriptPath

    # Calculate timeout from script timing fields (or use explicit -TimeoutSeconds)
    $scriptTimeout = $TimeoutSeconds
    if ($TimeoutSeconds -eq 300) {
        $srcForTimeout = if ($resolvedPath -ne $scriptPath) { $resolvedPath } else { $scriptPath }
        $scriptTimeout = Get-ScriptTimeoutSeconds -ScriptPath $srcForTimeout
        Write-Status "Calculated timeout: ${scriptTimeout}s (from script timing fields)"
    }

    if ($isLauncherScript) {
        # -- Launcher script: launch SetupActivity + send SETUP_AUTOMATE -----

        Write-Status "Launcher script detected -- using SetupActivity flow"
        Stop-AppAndWait
        Reset-GameState
        Adb -AdbArgs @("logcat", "-c") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_log.jsonl") | Out-Null

        Write-Status "Launching SetupActivity..."
        Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
        if (-not (Wait-SetupActivityReady)) {
            Write-Status "FAIL: SetupActivity not responding" "Red"
            $allPassed = $false
            if ($gameList.Count -gt 1) { continue }
            exit 1
        }

        Write-Status "Sending SETUP_AUTOMATE broadcast for: $ScriptName"
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_AUTOMATE", "--es", "script", $ScriptName) | Out-Null

        $passed = Watch-AutomationResult -TimeoutSeconds $scriptTimeout -IsLauncherScript
    } else {
        # -- Game script: launch game + send AUTOMATE broadcast -----------

        $extraArgs = @()
        if ($gameId -eq "d1") {
            Write-Status "Launching as D1"
            $extraArgs = @("--es", "game", "d1")
        }

        $preLaunch = {
            Reset-GameState
        }

        $launchParams = @{
            ExtraLaunchArgs = $extraArgs
            PreLaunchScript = $preLaunch
            Game = $gameId
        }
        if ($skipGameData) { $launchParams.SkipGameData = $true }

        if (-not (Start-GameWithRetry @launchParams)) {
            $allPassed = $false
            if ($gameList.Count -gt 1) { Write-Status "FAIL for $($gameId.ToUpper())" "Red"; continue }
            exit 1
        }

        Start-Sleep -Seconds 1
        Adb -AdbArgs @("logcat", "-c") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_log.jsonl") | Out-Null
        Write-Status "Sending automation broadcast for: $ScriptName"
        Adb -AdbArgs @("shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE", "--es", "script", $ScriptName) | Out-Null

        $passed = Watch-AutomationResult -TimeoutSeconds $scriptTimeout
    }

    if (-not $passed) {
        $allPassed = $false
        if ($gameList.Count -gt 1) { Write-Status "FAIL for $($gameId.ToUpper())" "Red"; continue }
        exit 1
    }

    # -- Step 7: Dump introspection -------------------------------

    Write-Status "Dumping final introspection state..."
    $intro = Get-GameIntrospection
    if ($intro) {
        Write-Host ($intro | ConvertTo-Json -Depth 10 -Compress)
    }

    if ($gameList.Count -gt 1) {
        Write-Status "PASS for $($gameId.ToUpper())" "Green"
        # Force-stop between game runs
        Stop-AppAndWait
    }
}

if (-not $allPassed) { exit 1 }
exit 0
