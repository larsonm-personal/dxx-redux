#!/usr/bin/env pwsh
# run_test.ps1 -- Run an automation test script on the Android emulator with health checks.
#
# Usage:
#   .\android\helpers\run_test.ps1 test_death.json5
#   .\android\helpers\run_test.ps1 test_death.json5 -Install   # also install APK first
#   .\android\helpers\run_test.ps1 test_axis_mapping.json5 -Game d1  # force a specific game
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
$helpersDir = Split-Path -Parent $PSCommandPath
$scriptDir = Split-Path -Parent $helpersDir
. (Join-Path $helpersDir "test_helpers.ps1")

function Push-TestScriptToDevice {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DeviceName
    )

    Write-Status "Pushing test script: $DeviceName"
    $devicePrefix = $DeviceName -replace '\.run-[^.]+\.json5$', ''
    Adb -AdbArgs @(
        "shell", "run-as", $script:PACKAGE, "find", "files", "-maxdepth", "1",
        "-name", "'$devicePrefix.run-*.json5'", "-delete"
    ) | Out-Null
    Adb -AdbArgs @("push", $SourcePath, "/data/local/tmp/$DeviceName") | Out-Null
    for ($stageAttempt = 1; $stageAttempt -le 3; $stageAttempt++) {
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "mkdir", "-p", "files") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "cp", "/data/local/tmp/$DeviceName", "files/$DeviceName") | Out-Null

        $staged = Adb-Timeout -AdbArgs @("shell", "run-as", $script:PACKAGE, "ls", "files/$DeviceName") -Seconds 5
        if ($staged -and $staged -notmatch 'No such file') {
            Adb -AdbArgs @("shell", "rm", "-f", "/data/local/tmp/$DeviceName") | Out-Null
            return $true
        }

        if ($stageAttempt -lt 3) {
            Write-Status "Test script stage check missed, retrying $($stageAttempt + 1)/3..." "Yellow"
        }
    }

    Write-Status "FAIL: Could not stage test script on device: $DeviceName" "Red"
    Adb -AdbArgs @("shell", "rm", "-f", "/data/local/tmp/$DeviceName") | Out-Null
    return $false
}

function Send-SetupCommand {
    param([Parameter(Mandatory = $true)][string]$Command)

    Adb -AdbArgs @(
        "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_COMMAND",
        "--es", "command", $Command
    ) | Out-Null
}

# -- Preflight script metadata --------------------------------

$scriptPath = Join-RegressionPath $scriptDir "game_scripts" $ScriptName
$gameList = Get-ScriptGameInfo -ScriptPath $scriptPath
$isStandaloneScript = Get-ScriptStandalone -ScriptPath $scriptPath

if (-not $isStandaloneScript) {
    Write-Status "WARNING: $ScriptName is a support script (not standalone). It is normally run by another test" "Yellow"
    if ($Params.Count -eq 0) {
        $rawScriptText = Get-Content -Path $scriptPath -Raw
        $rawPlaceholders = @([regex]::Matches($rawScriptText, '\$\{[A-Za-z0-9_]+\}') | ForEach-Object { $_.Value } | Select-Object -Unique)
        if ($rawPlaceholders.Count -gt 0) {
            Write-Status "FAIL: support template has unresolved placeholders: $($rawPlaceholders -join ',')" "Red"
            Write-Status "Run this template through its wrapper instead of launching it directly" "Yellow"
            exit 1
        }
    }
}

# -- Step 1: Health check -------------------------------------

Ensure-EmulatorHealthy

# -- Step 2: Install APK if requested ------------------------

if ($Install) {
    $apk = Join-RegressionPath $scriptDir "app" "build" "outputs" "apk" "debug" "app-debug.apk"
    if (-not (Test-Path $apk)) {
        Write-Status "FAIL: APK not found at $apk" "Red"
        exit 1
    }
    Write-Status "Installing APK..."
    if (-not (Install-ApkOnDevice)) {
        Write-Status "FAIL: APK install failed" "Red"
        exit 1
    }
}

# -- Step 3: Determine which game(s) to run -------------------

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
    $isInteractive = [Environment]::UserInteractive -and
    -not ([Environment]::GetCommandLineArgs() -match '-NonInteractive')
    if ($scriptParams) {
        foreach ($prop in $scriptParams.PSObject.Properties) {
            $pName = $prop.Name
            if (-not $resolvedParams.ContainsKey($pName)) {
                $pDef = $prop.Value
                $optKeys = @($pDef.options.PSObject.Properties.Name)
                $label = if ($pDef.label) { $pDef.label } else { $pName }
                if ($isInteractive) {
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
                } else {
                    Write-Status "Non-interactive: defaulting $label to $($optKeys[0])" "Yellow"
                    $resolvedParams[$pName] = $optKeys[0]
                }
            }
        }
    }

    # -- Resolve script (variable substitution + conditional filtering) --

    $resolvedPath = Resolve-TestScript -ScriptPath $scriptPath -GameId $gameId -Params $resolvedParams
    $runId = [guid]::NewGuid().ToString("N")
    $scriptStem = [System.IO.Path]::GetFileNameWithoutExtension($ScriptName)
    $pushName = "$scriptStem.run-$runId.json5"
    if ($resolvedPath -ne $scriptPath) {
        # Push the resolved file instead, but keep the original name on device
        $pushSrc = $resolvedPath
    } else {
        $pushSrc = $scriptPath
    }
    $resolvedText = Get-Content -Path $pushSrc -Raw
    $unresolved = @([regex]::Matches($resolvedText, '\$\{[A-Za-z0-9_]+\}') | ForEach-Object { $_.Value } | Select-Object -Unique)
    if ($unresolved.Count -gt 0) {
        $hint = if (-not $isStandaloneScript) {
            " This support template should be run through its wrapper, not directly."
        } else {
            ""
        }
        Write-Status "FAIL: unresolved script placeholders: $($unresolved -join ',').$hint" "Red"
        $allPassed = $false
        if ($gameList.Count -gt 1) { continue }
        exit 1
    }

    # -- Resolve declarative game data deps (if present) ----------

    # Build vars for dep substitution (game vars + param option vars)
    $depVars = @{} + $resolvedParams
    if ($scriptParams -and $resolvedParams.Count -gt 0) {
        foreach ($pName in $resolvedParams.Keys) {
            $pValue = $resolvedParams[$pName]
            $paramDef = $scriptParams.$pName
            if ($paramDef -and $paramDef.options -and $paramDef.options.$pValue) {
                foreach ($prop in $paramDef.options.$pValue.PSObject.Properties) {
                    $depVars[$prop.Name] = $prop.Value
                }
            }
        }
    }
    $skipGameData = $false
    $deps = Get-ScriptDeps -ScriptPath $scriptPath -Vars $depVars
    if ($deps) {
        Write-Status "Resolving $($deps.Count) declared game data deps..."
        if (-not (Resolve-GameDataDeps -Deps $deps)) {
            if (-not $isInteractive) {
                Write-Status "SKIP: deps unavailable in non-interactive mode" "Yellow"
                Write-Host "RESULT: SKIP (declared game-data dependencies unavailable)"
                exit 2
            }
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
        $scriptEstimate = Get-ScriptTimeoutSeconds -ScriptPath $srcForTimeout
        $scriptTimeout = [Math]::Max($TimeoutSeconds, $scriptEstimate)
        if ($scriptEstimate -gt $TimeoutSeconds) {
            Write-Status "Calculated timeout: ${scriptTimeout}s (from script timing fields)"
        } else {
            Write-Status "Using default timeout: ${scriptTimeout}s (script estimate: ${scriptEstimate}s)"
        }
    }
    if ($isLauncherScript -and $TimeoutSeconds -eq 300 -and $scriptTimeout -lt 600) {
        $scriptTimeout = 600
        Write-Status "Using launcher minimum timeout: ${scriptTimeout}s"
    }

    if ($isLauncherScript) {
        # -- Launcher script: launch SetupActivity + send SETUP_AUTOMATE -----

        Write-Status "Launcher script detected -- using SetupActivity flow"
        Stop-AppAndWait
        Reset-GameState
        Adb -AdbArgs @("logcat", "-c") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json", "files/automation_result.json.tmp") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_log.jsonl") | Out-Null

        $launcherReady = $false
        $launcherRecoveryFailed = $false
        for ($launcherAttempt = 1; $launcherAttempt -le 2; $launcherAttempt++) {
            Write-Status "Launching SetupActivity..."
            Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
            if (Wait-SetupActivityReady) {
                $launcherReady = $true
                break
            }

            Write-Status "SetupActivity not responding after 30s" "Yellow"
            if ($launcherAttempt -ge 2) {
                break
            }

            if (-not (Invoke-LauncherStartupRecovery -Reason "SetupActivity launch timed out for $ScriptName")) {
                $launcherRecoveryFailed = $true
                break
            }

            Install-ApkOnDevice | Out-Null
            if ($deps) {
                Write-Status "Re-provisioning declared game data deps after launcher recovery"
                if (-not (Resolve-GameDataDeps -Deps $deps)) {
                    $launcherRecoveryFailed = $true
                    break
                }
            } elseif (-not $skipGameData -and -not (Ensure-GameDataOnDevice -Game $gameId)) {
                $launcherRecoveryFailed = $true
                break
            }

            Stop-AppAndWait
            Reset-GameState
            Adb -AdbArgs @("logcat", "-c") | Out-Null
            Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json", "files/automation_result.json.tmp") | Out-Null
            Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_log.jsonl") | Out-Null
        }

        if (-not $launcherReady) {
            if ($launcherRecoveryFailed) {
                Write-Status "FAIL: SetupActivity recovery could not restore launcher prerequisites" "Red"
            } else {
                Write-Status "FAIL: SetupActivity not responding" "Red"
            }
            $allPassed = $false
            if ($gameList.Count -gt 1) { continue }
            exit 1
        }

        Write-Status "Clearing save files for clean launcher state"
        Send-SetupCommand -Command "clear_save_files"
        Start-Sleep -Milliseconds 250

        if (-not (Push-TestScriptToDevice -SourcePath $pushSrc -DeviceName $pushName)) {
            $allPassed = $false
            if ($gameList.Count -gt 1) { continue }
            exit 1
        }

        Write-Status "Sending SETUP_AUTOMATE broadcast for: $ScriptName"
        Adb -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.SETUP_AUTOMATE",
            "--es", "script", $pushName, "--es", "run_id", $runId
        ) | Out-Null

        $passed = Watch-AutomationResult -TimeoutSeconds $scriptTimeout -IsLauncherScript -ExpectedRunId $runId
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

        if (-not (Push-TestScriptToDevice -SourcePath $pushSrc -DeviceName $pushName)) {
            $allPassed = $false
            if ($gameList.Count -gt 1) { Write-Status "FAIL for $($gameId.ToUpper())" "Red"; continue }
            exit 1
        }

        Start-Sleep -Seconds 1
        Adb -AdbArgs @("logcat", "-c") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_result.json", "files/automation_result.json.tmp") | Out-Null
        Adb -AdbArgs @("shell", "run-as", $script:PACKAGE, "rm", "-f", "files/automation_log.jsonl") | Out-Null
        Write-Status "Sending automation broadcast for: $ScriptName"
        Adb -AdbArgs @(
            "shell", "am", "broadcast", "-a", "com.dxxredux.AUTOMATE",
            "--es", "script", $pushName, "--es", "run_id", $runId
        ) | Out-Null

        $passed = Watch-AutomationResult -TimeoutSeconds $scriptTimeout -ExpectedRunId $runId
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
