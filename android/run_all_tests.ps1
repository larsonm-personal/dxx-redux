#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Unattended sequential test runner with automatic infrastructure setup.

.DESCRIPTION
    Discovers all json5 and ps1 tests, automatically provisions required
    infrastructure (emulators, matchmaking server, Docker NAT containers),
    runs tests sequentially, and produces a summary report.

        Infrastructure is brought up on demand and torn down at the end:
            1. No-infra tests (host-side comparisons, unit tests, server integration)
            2. Server-only tests (matchmaking bot/client checks)
            3. Single emulator started, APK installed, game data pushed
            4. Single-emulator tests (json5 automation + ps1 emulator tests)
            5. Extract regressions (single emulator + source import checks)
            6. Second emulator started, APK installed, game data pushed
            7. Matchmaking server started for the dual-emulator tier
            8. Two-emulator tests (multiplayer and LAN)
            9. Cleanup in reverse order

.PARAMETER Filter
    Glob filter for test names (e.g. "test_death*").

.PARAMETER IncludeManual
    Include tests that are normally skipped (dual-emu setup, manual tests).

.PARAMETER StopOnFail
    Stop after the first failure instead of continuing.

.PARAMETER ReportDir
    Directory for per-test output logs (default: temp\test_reports).

.PARAMETER TestTimeoutSeconds
    Maximum wall-clock seconds per test before killing it (default: 120).

.PARAMETER SkipDocker
    Skip Docker NAT tests even if Docker is available.

.EXAMPLE
    .\run_all_tests.ps1
    .\run_all_tests.ps1 -Filter "test_death*"
    .\run_all_tests.ps1 -StopOnFail
    .\run_all_tests.ps1 -SkipDocker
#>

param(
    [string]$Filter,
    [switch]$IncludeManual,
    [switch]$StopOnFail,
    [string]$ReportDir,
    [int]$TestTimeoutSeconds = 120,
    [switch]$SkipDocker
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $PSCommandPath
$repoRoot = Split-Path $scriptDir

. "$scriptDir\test_helpers.ps1"

# -- Report directory --

if (-not $ReportDir) {
    $ReportDir = Join-Path $repoRoot "temp\test_reports"
}
New-Item -Path $ReportDir -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$reportFile = Join-Path $ReportDir "report_$timestamp.md"

# -- Environment probes (non-provisioning) --

function Test-SingleEmulator {
    $out = Adb-Timeout -AdbArgs @("devices") -Seconds 5
    if ($out -match "emulator-\d+\s+device") { return $true }
    return $false
}

function Test-TwoEmulators {
    $out = Adb-Timeout -AdbArgs @("devices") -Seconds 5
    if (-not $out) { return $false }
    $m = [regex]::Matches($out, "emulator-\d+\s+device")
    return ($m.Count -ge 2)
}

function Test-MatchmakingServer {
    try {
        $tcp = [System.Net.Sockets.TcpClient]::new()
        $tcp.Connect("127.0.0.1", 9000)
        $tcp.Close()
        return $true
    } catch { return $false }
}

function Test-GameDataAvailable {
    $gameDataDir = Join-Path $repoRoot "game_data"
    $specs = Get-ChildItem -Path $gameDataDir -Recurse -Filter "extract_regression.json5" -ErrorAction SilentlyContinue
    return ($specs -and $specs.Count -gt 0)
}

function Test-DockerAvailable {
    if ($SkipDocker) { return $false }
    try {
        $null = docker version --format '{{.Server.Version}}' 2>&1
        return ($LASTEXITCODE -eq 0)
    } catch { return $false }
}

function Get-RegressionDemoCount {
    $demoRoot = Join-Path $scriptDir "regression_demos"
    if (-not (Test-Path -LiteralPath $demoRoot)) {
        return 0
    }

    return @(
        Get-ChildItem -Path $demoRoot -Recurse -Filter "*.dximdemo" -File -ErrorAction SilentlyContinue
    ).Count
}

function Get-TestBaseName {
    param([string]$TestName)

    switch ($TestName) {
        "test_input_demo_regressions_graphics" { return "test_input_demo_regressions" }
        default { return $TestName }
    }
}

function Test-MatchesRequestedFilter {
    param(
        [hashtable]$Test,
        [string]$RequestedFilter
    )

    if (-not $RequestedFilter) {
        return $true
    }

    return ($Test.Name -like $RequestedFilter -or $Test.BaseName -like $RequestedFilter)
}

# -- Test catalog --

# Tests always skipped in unattended mode
$manualTests = @(
    "test_keyboard_manual"     # by-hand keyboard interaction test
    "test_dual_emu"            # interactive menu
    "test_dual_emu_setup"      # interactive setup
    "test_lan_discovery"       # host-side bot for by-hand discovery checks
    "test_manual_lan_coop"     # launches manual LAN coop setup
    "test_skip_every_launch_button_manual_unified" # requires an external adb tap during the intro
)

# Infrastructure requirement classification
$twoEmuTests = @("test_mp", "test_lan", "test_lan_discovery", "test_lan_broadcast", "test_lan_lobby_discovery")
$serverTests = @("test_bot_client")

# Per-test timeout overrides (seconds) for multi-phase tests
$testTimeouts = @{
    "test_autoselect_crash_unified"       = 240
    "test_autosave_resume_unified"        = 300
    "test_autosave_resume_missing_pilot_unified" = 300
    "test_axis_mapping"                   = 240
    "test_engine_prefs_unified"           = 240
    "test_extract"                        = 240
    "test_gog_installer_d1_unified"       = 420
    "test_gog_installer_redbook_unified"  = 420
    "test_gradle_unit_tests"              = 600
    "test_input_demo_determinism_matrix"  = 600
    "test_input_demo_regressions"         = 900
    "test_input_demo_regressions_graphics" = 900
    "test_saf_archiver"                   = 360
    "test_all_extracts"                   = 300
    "test_mp"                             = 240
}
$extractTests = @(
    "test_all_extracts",
    "test_extract",
    "test_gog_installer_d1_unified",
    "test_gog_installer_redbook_unified"
)  # single emulator + game data, run before the dual-emulator tier
$noInfraTests = @(
    "test_cue_iso",
    "test_fpcalc_and_acoustid",
    "test_gradle_unit_tests",
    "test_input_demo_determinism_matrix",
    "test_input_demo_regressions",
    "test_input_demo_regressions_graphics",
    "test_input_demo_runtime_smoke",
    "test_server_integration",
    "test_input_demo_state_trace_compare",
    "test_input_demo_rng_trace_compare"
)

$allTests = @()

# json5 game-automation scripts (run via run_test.ps1)
$gameScriptsDir = Join-Path $scriptDir "game_scripts"
$json5Files = @(Get-ChildItem -Path $gameScriptsDir -Filter "test_*.json5" -File -ErrorAction SilentlyContinue | Sort-Object Name)
foreach ($t in $json5Files) {
    if (-not (Get-ScriptStandalone -ScriptPath $t.FullName)) {
        continue  # skip template scripts that need a caller
    }
    $name = $t.BaseName
    $allTests += @{
        Name = $name
        BaseName = $name
        Type = "json5"
        Path = $t.FullName
        Requires = "emulator"
        TimeoutSeconds = if ($testTimeouts.ContainsKey($name)) { $testTimeouts[$name] } else { 0 }
    }
}

# ps1 integration tests
$testsDir = Join-Path $scriptDir "tests"
$ps1Files = @(Get-ChildItem -Path $testsDir -Filter "test_*.ps1" -File -ErrorAction SilentlyContinue | Sort-Object Name)
foreach ($t in $ps1Files) {
    $name = $t.BaseName
    $baseName = Get-TestBaseName -TestName $name
    $req = "none"
    if ($name -in $twoEmuTests) { $req = "two_emulators" }
    elseif ($name -in $serverTests) { $req = "server" }
    elseif ($name -in $extractTests) { $req = "extract" }
    elseif ($name -in $noInfraTests) { $req = "none" }
    else { $req = "emulator" }

    $timeoutSeconds = 0
    if ($testTimeouts.ContainsKey($name)) {
        $timeoutSeconds = $testTimeouts[$name]
    } elseif ($testTimeouts.ContainsKey($baseName)) {
        $timeoutSeconds = $testTimeouts[$baseName]
    }

    $allTests += @{
        Name = $name
        BaseName = $baseName
        Type = "ps1"
        Path = $t.FullName
        Requires = $req
        TimeoutSeconds = $timeoutSeconds
        DemoRunMode = if ($baseName -eq "test_input_demo_regressions") {
            if ($name -eq "test_input_demo_regressions_graphics") { "graphics" } else { "headless" }
        } else {
            ""
        }
    }
}

# Apply filter
if ($Filter) {
    $allTests = @($allTests | Where-Object { Test-MatchesRequestedFilter -Test $_ -RequestedFilter $Filter })
}

# Separate manual from runnable
$manualSkipped = @()
$runnableTests = @()
foreach ($test in $allTests) {
    if ((-not $IncludeManual) -and ($test.Name -in $manualTests)) {
        $manualSkipped += @{ Name = $test.Name; Reason = "manual/interactive"; Type = $test.Type }
    } else {
        $runnableTests += $test
    }
}

function Get-TestExecutionOrderKey {
    param([hashtable]$Test)

    switch ($Test.Name) {
        "test_quick_record_classic_sidecar_stage" { return "test_quick_record_classic_sidecar_00_stage" }
        "test_quick_record_classic_sidecar_install" { return "test_quick_record_classic_sidecar_01_install" }
        default { return $Test.Name }
    }
}

function Sort-TestsForExecution {
    param([object[]]$Tests)

    return @($Tests | Sort-Object @{ Expression = { Get-TestExecutionOrderKey $_ } }, @{ Expression = { $_.Name } })
}

# Group by infrastructure tier
$tierNone = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "none" })
$tierServer = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "server" })
$tierSingleEmu = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "emulator" })
$tierDualEmu = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "two_emulators" })
$tierExtract = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "extract" })

$selectedRegressionDemoTests = @($runnableTests | Where-Object { $_.BaseName -eq "test_input_demo_regressions" })
$selectedRegressionDemoModes = @($selectedRegressionDemoTests | ForEach-Object { $_.DemoRunMode } | Where-Object { $_ } | Sort-Object -Unique)
$regressionDemoCount = Get-RegressionDemoCount
$selectedRegressionDemoReplayCount = $regressionDemoCount * $selectedRegressionDemoTests.Count

function Restart-AdbServer {
    Write-Status "Restarting ADB server..." "DarkGray"
    Get-Process adb -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    & $script:ADB start-server 2>$null
    Start-Sleep -Seconds 2
}

function Get-OnlineEmulatorSerials {
    $devices = Adb-Timeout -AdbArgs @("devices") -Seconds 5
    if (-not $devices) {
        return @()
    }

    return @([regex]::Matches($devices, "(emulator-\d+)\s+device") |
            ForEach-Object { $_.Groups[1].Value } |
            Sort-Object -Unique)
}

function Test-SingleEmulatorFailureNeedsRecovery {
    param([hashtable]$Result)

    if (-not $Result -or $Result.Status -eq "PASS") {
        return $false
    }

    if ($Result.Status -eq "TIMEOUT") {
        return $true
    }

    if (-not $Result.LogFile -or -not (Test-Path $Result.LogFile)) {
        return $false
    }

    $logText = Get-Content $Result.LogFile -Raw -ErrorAction SilentlyContinue
    if (-not $logText) {
        return $false
    }

    return $logText -match 'SetupActivity not responding|SetupActivity not responding after 30s|Game never started after \d+ attempts|SetupActivity recovery could not restore launcher prerequisites|Launcher recovery failed|Emulator could not be restored|Can''t find service: package|no emulator device found|shell unresponsive'
}

function Recover-SingleEmulatorEnvironment {
    param([ref]$SerialRef)

    Write-Status "Single-emulator recovery: restarting primary emulator and reprovisioning app/data" "Yellow"
    Restart-AdbServer

    $healthScript = Join-Path $scriptDir "emu_health.ps1"
    & $healthScript -Restart -Wait -ForceRestart -TimeoutSeconds 180 -AvdName $script:PRIMARY_AVD_NAME
    $healthExit = $LASTEXITCODE
    if ($healthExit -ne 0 -and $healthExit -ne 2) {
        Write-Status "Single-emulator recovery failed: emulator restart exit $healthExit" "Red"
        return $false
    }

    $serial = Get-OnlineEmulatorSerials | Select-Object -First 1
    if (-not $serial) {
        Write-Status "Single-emulator recovery failed: no online emulator after restart" "Red"
        return $false
    }

    Install-AppAndData -Serial $serial
    $SerialRef.Value = $serial
    Write-Status "Single-emulator recovery complete" "Green"
    return $true
}

function Recover-DualEmulatorEnvironment {
    param(
        [ref]$PrimarySerialRef,
        [ref]$SecondarySerialRef
    )

    Write-Status "Dual-emulator recovery: forcing clean emulator recycle and reprovisioning app/data" "Yellow"
    Restart-AdbServer

    if ($script:autoServerProc -and -not $script:autoServerProc.HasExited) {
        try { $script:autoServerProc.Kill() } catch {}
        try { $script:autoServerProc.WaitForExit(5000) } catch {}
        $script:autoServerProc = $null
    }

    $healthScript = Join-Path $scriptDir "emu_health.ps1"
    & $healthScript -Restart -Wait -ForceRestart -TimeoutSeconds 240 -AvdName $script:PRIMARY_AVD_NAME
    $healthExit = $LASTEXITCODE
    if ($healthExit -ne 0 -and $healthExit -ne 2) {
        Write-Status "Dual-emulator recovery failed: primary emulator restart exit $healthExit" "Red"
        return $false
    }

    if (-not (Start-SecondEmulator)) {
        Write-Status "Dual-emulator recovery failed: could not start second emulator" "Red"
        return $false
    }

    $serials = Get-OnlineEmulatorSerials
    $primarySerial = if ($serials -contains $script:PRIMARY_EMULATOR_SERIAL) {
        $script:PRIMARY_EMULATOR_SERIAL
    } else {
        $serials | Select-Object -First 1
    }
    $secondarySerial = if ($serials -contains $script:SECONDARY_EMULATOR_SERIAL) {
        $script:SECONDARY_EMULATOR_SERIAL
    } else {
        $serials | Select-Object -Last 1
    }

    if (-not $primarySerial -or -not $secondarySerial -or $primarySerial -eq $secondarySerial) {
        Write-Status "Dual-emulator recovery failed: online emulator set incomplete after restart" "Red"
        return $false
    }

    Install-AppAndData -Serial $primarySerial
    Install-AppAndData -Serial $secondarySerial

    $PrimarySerialRef.Value = $primarySerial
    $SecondarySerialRef.Value = $secondarySerial

    if (-not (Test-MatchmakingServer)) {
        $script:autoServerProc = Start-MatchmakingServer
        if ($null -eq $script:autoServerProc) {
            Write-Status "Dual-emulator recovery failed: could not restart matchmaking server" "Red"
            return $false
        }
    }

    Write-Status "Dual-emulator recovery complete" "Green"
    return $true
}

function Invoke-WithAndroidSerial {
    param(
        [string]$Serial,
        [scriptblock]$ScriptBlock
    )

    $prevSerial = $env:ANDROID_SERIAL
    if ($Serial) {
        $env:ANDROID_SERIAL = $Serial
    }

    try {
        return (& $ScriptBlock)
    } finally {
        if ($prevSerial) {
            $env:ANDROID_SERIAL = $prevSerial
        } else {
            Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue
        }
    }
}

function Invoke-SetupActivityPreflight {
    param(
        [string]$Serial,
        [switch]$RequireStandardGameData
    )

    $serialLabel = if ($Serial) { $Serial } else { 'default-emulator' }
    Write-Status "Preflight: verifying SetupActivity on $serialLabel..." "DarkGray"

    $ready = Invoke-WithAndroidSerial -Serial $Serial -ScriptBlock {
        Stop-AppAndWait
        Reset-GameState
        Adb -AdbArgs @("logcat", "-c") | Out-Null
        Adb -AdbArgs @("shell", "am", "start", "-n", "$($script:PACKAGE)/$($script:ACTIVITY)") | Out-Null
        if (-not (Wait-SetupActivityReady -TimeoutSeconds 30)) {
            return $false
        }

        $setup = Get-SetupIntrospection
        if (-not $setup) {
            return $false
        }

        if ($RequireStandardGameData) {
            $d1Ready = $false
            $d2Ready = $false
            if ($setup.d1) { $d1Ready = [bool]$setup.d1.ready }
            if ($setup.d2) { $d2Ready = [bool]$setup.d2.ready }
            if (-not $d1Ready -or -not $d2Ready) {
                Write-Status "Preflight: standard game data missing on device (d1=$d1Ready d2=$d2Ready)" "Red"
                return $false
            }
        }

        return $true
    }

    if ($ready) {
        Write-Status "Preflight: SetupActivity ready on $serialLabel" "Green"
    }

    return $ready
}

function Invoke-PrimaryEmulatorPreflight {
    param([switch]$RequireStandardGameData)

    $healthScript = Join-Path $scriptDir "emu_health.ps1"

    for ($attempt = 1; $attempt -le 2; $attempt++) {
        if (-not (Test-SingleEmulator)) {
            $emu1Started = Start-SingleEmulator
            if (-not $emu1Started) {
                return $null
            }
            $script:startedEmu1 = $true
        }

        Ensure-EmulatorHealthy | Out-Null

        $serial = Get-OnlineEmulatorSerials | Select-Object -First 1
        if (-not $serial) {
            Write-Status "Preflight: no online primary emulator found" "Red"
            return $null
        }

        Install-AppAndData -Serial $serial
        if (Invoke-SetupActivityPreflight -Serial $serial -RequireStandardGameData:$RequireStandardGameData) {
            return $serial
        }

        if ($attempt -lt 2) {
            Write-Status "Preflight: restarting primary emulator and retrying launcher readiness" "Yellow"
            & $healthScript -Restart -Wait -ForceRestart -TimeoutSeconds 180 -AvdName $script:PRIMARY_AVD_NAME
            $healthExit = $LASTEXITCODE
            if ($healthExit -ne 0 -and $healthExit -ne 2) {
                Write-Status "Preflight: emulator restart failed (exit $healthExit)" "Red"
                return $null
            }
            $script:startedEmu1 = $true
        }
    }

    return $null
}

function Invoke-SecondaryEmulatorPreflight {
    param([switch]$RequireStandardGameData)

    if (-not (Test-TwoEmulators)) {
        $emu2Started = Start-SecondEmulator
        if (-not $emu2Started) {
            return $null
        }
        $script:startedEmu2 = $true
    }

    $serials = Get-OnlineEmulatorSerials
    if ($serials.Count -lt 2) {
        Write-Status "Preflight: second emulator not visible in adb" "Red"
        return $null
    }

    $serial = $serials | Select-Object -Last 1
    Install-AppAndData -Serial $serial
    if (-not (Invoke-SetupActivityPreflight -Serial $serial -RequireStandardGameData:$RequireStandardGameData)) {
        return $null
    }

    return $serial
}

function Invoke-SuitePreflight {
    $needsPrimaryEmulator = ($tierSingleEmu.Count + $tierDualEmu.Count + $tierExtract.Count) -gt 0
    $needsServer = $tierServer.Count -gt 0
    $needsExtractData = $tierExtract.Count -gt 0
    $needsStandardGameData = ($tierSingleEmu.Count + $tierDualEmu.Count) -gt 0

    if (-not ($needsPrimaryEmulator -or $needsServer -or $needsExtractData)) {
        return $true
    }

    Write-Host ""
    Write-Host "== Suite preflight ==" -ForegroundColor Cyan

    if ($needsExtractData -and -not (Test-GameDataAvailable)) {
        Write-Host "FAIL: Extract tests are selected but the required CD-image game data is not available" -ForegroundColor Red
        return $false
    }

    if ($needsPrimaryEmulator) {
        Restart-AdbServer
        $preflightEmu1 = Invoke-PrimaryEmulatorPreflight -RequireStandardGameData:$needsStandardGameData
        if (-not $preflightEmu1) {
            Write-Host "FAIL: Suite preflight could not prepare a healthy primary emulator" -ForegroundColor Red
            return $false
        }
    }

    if ($needsServer) {
        if (-not (Test-MatchmakingServer)) {
            $script:autoServerProc = Start-MatchmakingServer
            if ($null -eq $script:autoServerProc) {
                Write-Host "FAIL: Suite preflight could not start the matchmaking server" -ForegroundColor Red
                return $false
            }
        }
    }

    Write-Host "  Preflight OK" -ForegroundColor Green
    Write-Host ""
    return $true
}

function ConvertTo-ArgumentText {
    param([string[]]$Arguments)

    if (-not $Arguments -or $Arguments.Count -eq 0) {
        return ""
    }

    return ($Arguments | ForEach-Object {
            if ($_ -match '[\s"]') {
                '"' + ($_ -replace '"', '\"') + '"'
            } else {
                $_
            }
        }) -join ' '
}

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  DXX-Redux Unattended Test Suite" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tests found: $($allTests.Count) total, $($runnableTests.Count) runnable, $($manualSkipped.Count) manual-skipped" -ForegroundColor White
Write-Host "  Tier 0 (no infra):       $($tierNone.Count)"
Write-Host "  Tier 1 (server only):    $($tierServer.Count)"
Write-Host "  Tier 2 (single emu):     $($tierSingleEmu.Count)"
Write-Host "  Tier 3 (extract):        $($tierExtract.Count)"
Write-Host "  Tier 4 (dual emu):       $($tierDualEmu.Count)"
if ($selectedRegressionDemoModes.Count -gt 0) {
    Write-Host "  Demo regressions:       $regressionDemoCount demo(s), modes: $($selectedRegressionDemoModes -join '+'), replay runs: $selectedRegressionDemoReplayCount"
} else {
    Write-Host "  Demo regressions:       0 replay runs selected"
}
Write-Host ""

if ($runnableTests.Count -eq 0) {
    Write-Host "No runnable tests found" -ForegroundColor Yellow
    exit 0
}

# -- Build APK if any emulator tests will run --

$needsApk = ($tierSingleEmu.Count + $tierDualEmu.Count + $tierExtract.Count) -gt 0
if ($needsApk) {
    Write-Host "== Building debug APK ==" -ForegroundColor Cyan
    Push-Location $scriptDir
    try {
        & .\gradlew.bat assembleDebug --console=plain 2>&1 |
            Where-Object { $_ -match "BUILD |FAIL|error:" } |
            ForEach-Object { Write-Host "  $_" }
        if ($LASTEXITCODE -ne 0) {
            Write-Host "FAIL: APK build failed" -ForegroundColor Red
            exit 1
        }
        Write-Host "  Build OK" -ForegroundColor Green
    } finally {
        Pop-Location
    }
    Write-Host ""
}

if (-not (Invoke-SuitePreflight)) {
    exit 1
}

# -- Execution helpers --

$runTestScript = Join-Path $scriptDir "run_test.ps1"
$results = @()
$passCount = 0
$failCount = 0
$timeoutCount = 0
$infraSkipped = @()
$stopEarly = $false
$totalSw = [System.Diagnostics.Stopwatch]::StartNew()

# Tracked infra for cleanup
$script:autoServerProc = $null
$script:startedEmu1 = $false
$script:startedEmu2 = $false
$script:startedDocker = $false

function Invoke-SingleTest {
    param([hashtable]$Test)
    $name = $Test.Name
    $logFile = Join-Path $ReportDir "${name}_${timestamp}.log"

    # Per-test timeout override for multi-phase tests that need more time
    $testTimeout = $TestTimeoutSeconds
    if ($Test.TimeoutSeconds) { $testTimeout = $Test.TimeoutSeconds }

    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  Running: $name  [$($Test.Type)]  (timeout: ${testTimeout}s)" -ForegroundColor White
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    if ($Test.Type -eq "json5") {
        $psScript = $runTestScript
        $psArguments = @("-ScriptName", [System.IO.Path]::GetFileName($Test.Path))
    } elseif ($Test.Name -eq "test_saf_archiver") {
        $psScript = $Test.Path
        $psArguments = @("-NoBuild")
    } else {
        $psScript = $Test.Path
        $psArguments = @()
    }
    $extraArguments = $null
    if ($Test -is [System.Collections.IDictionary]) {
        if ($Test.Contains('Arguments')) {
            $extraArguments = $Test['Arguments']
        }
    } elseif ($Test.PSObject.Properties['Arguments']) {
        $extraArguments = $Test.Arguments
    }
    if ($extraArguments) {
        $psArguments += $extraArguments
    }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "pwsh"
    $quotedScript = if ($psScript -match '[\s"]') {
        '"' + ($psScript -replace '"', '\"') + '"'
    } else {
        $psScript
    }
    $argumentText = ConvertTo-ArgumentText -Arguments $psArguments
    $psi.Arguments = "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File $quotedScript $argumentText"
    $psi.WorkingDirectory = $scriptDir
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $timedOut = $false
    $exitCode = 1
    try {
        $proc = [System.Diagnostics.Process]::Start($psi)
        $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
        $stderrTask = $proc.StandardError.ReadToEndAsync()

        if (-not $proc.WaitForExit($testTimeout * 1000)) {
            $timedOut = $true
            try { $proc.Kill($true) } catch { try { $proc.Kill() } catch {} }
            Start-Sleep -Seconds 1
            $exitCode = 1
        } else {
            $exitCode = $proc.ExitCode
        }

        $stdoutTask.Wait(5000) | Out-Null
        $stderrTask.Wait(5000) | Out-Null
        $stdout = if ($stdoutTask.IsCompleted) { $stdoutTask.Result } else { "" }
        $stderr = if ($stderrTask.IsCompleted) { $stderrTask.Result } else { "" }
        $proc.Dispose()

        $stdout | Out-File -FilePath $logFile -Encoding utf8
        if ($stderr) { $stderr | Out-File -FilePath $logFile -Append -Encoding utf8 }
        if ($timedOut) {
            "TIMEOUT: Test killed after ${testTimeout}s" | Out-File -FilePath $logFile -Append -Encoding utf8
        }

        $lines = ($stdout -split "`n" | Where-Object { $_.Trim() })
        $tail = $lines | Select-Object -Last 5
        foreach ($line in $tail) {
            Write-Host "    $($line.TrimEnd())" -ForegroundColor DarkGray
        }
    } catch {
        $exitCode = 1
        "EXCEPTION: $_" | Out-File -FilePath $logFile -Encoding utf8
        Write-Host "  EXCEPTION: $_" -ForegroundColor Red
    }

    $sw.Stop()
    $elapsed = $sw.Elapsed.ToString("mm\:ss")

    if ($timedOut) {
        $status = "TIMEOUT"; $color = "Yellow"; $script:timeoutCount++
    } elseif ($exitCode -eq 0) {
        $status = "PASS"; $color = "Green"
    } else {
        $status = "FAIL"; $color = "Red"
    }
    Write-Host "  $status ($elapsed)" -ForegroundColor $color

    if ($exitCode -eq 0) { $script:passCount++ } else { $script:failCount++ }

    $result = @{
        Name = $name; Type = $Test.Type; Status = $status
        ExitCode = $exitCode; Elapsed = $elapsed; LogFile = $logFile
    }
    $script:results += $result

    if ($StopOnFail -and $exitCode -ne 0) {
        Write-Host "  Stopping early (-StopOnFail)" -ForegroundColor Yellow
        $script:stopEarly = $true
    }

    return $result
}

function Get-ReportLogExcerpt {
    param(
        [string]$LogFile,
        [string]$Status
    )

    if (-not $LogFile -or -not (Test-Path -LiteralPath $LogFile)) {
        return @()
    }

    $lines = @(Get-Content -LiteralPath $LogFile -ErrorAction SilentlyContinue)
    if ($lines.Count -eq 0) {
        return @()
    }

    $patterns = @(
        'Running as D[12]',
        'FAIL for D[12]',
        'PASS for D[12]',
        'ASSERT_FAIL',
        'script","status":"FAIL',
        'TIMEOUT',
        'SetupActivity not responding',
        'SetupActivity recovery could not restore launcher prerequisites',
        'Launcher recovery failed',
        'Emulator could not be restored',
        "Can't find service: package",
        'no emulator device found',
        'shell unresponsive',
        'EXCEPTION:',
        'FAIL: APK build failed'
    )

    if ($Status -eq 'TIMEOUT') {
        $patterns = @('TIMEOUT waiting', 'TIMEOUT:', 'timed out') + $patterns
    }

    $selectedIndexes = [System.Collections.Generic.HashSet[int]]::new()
    for ($index = 0; $index -lt $lines.Count; $index++) {
        $line = $lines[$index]
        foreach ($pattern in $patterns) {
            if ($line -match $pattern) {
                $start = [Math]::Max(0, $index - 2)
                $end = [Math]::Min($lines.Count - 1, $index + 2)
                for ($cursor = $start; $cursor -le $end; $cursor++) {
                    $null = $selectedIndexes.Add($cursor)
                }
                break
            }
        }
    }

    if ($selectedIndexes.Count -eq 0) {
        return @($lines | Select-Object -Last 20)
    }

    $orderedIndexes = @($selectedIndexes | Sort-Object)
    if ($orderedIndexes.Count -gt 40) {
        $orderedIndexes = @($orderedIndexes | Select-Object -Last 40)
    }

    return @($orderedIndexes | ForEach-Object { $lines[$_] })
}

# ── Tier 0: no-infrastructure tests ─────────────────────────────────────

if ($tierNone.Count -gt 0 -and -not $stopEarly) {
    Write-Host ""
    Write-Host "== Tier 0: No-infrastructure tests ==" -ForegroundColor Cyan
    foreach ($test in $tierNone) {
        if ($stopEarly) { break }
        Invoke-SingleTest -Test $test
    }
}

# ── Tier 1: server-only tests ────────────────────────────────────────────

if ($tierServer.Count -gt 0 -and -not $stopEarly) {
    Write-Host ""
    Write-Host "== Tier 1: Server-only tests ==" -ForegroundColor Cyan

    if (-not (Test-MatchmakingServer)) {
        $script:autoServerProc = Start-MatchmakingServer
        $serverOk = ($null -ne $script:autoServerProc)
    } else {
        $serverOk = $true
    }

    if ($serverOk) {
        foreach ($test in $tierServer) {
            if ($stopEarly) { break }
            Invoke-SingleTest -Test $test
        }
    } else {
        foreach ($test in $tierServer) {
            $infraSkipped += @{ Name = $test.Name; Reason = "could not start matchmaking server"; Type = $test.Type }
        }
    }
}

# ── Tier 2: single-emulator tests ───────────────────────────────────────

if ($tierSingleEmu.Count -gt 0 -and -not $stopEarly) {
    Write-Host ""
    Write-Host "== Tier 2: Single-emulator tests ==" -ForegroundColor Cyan

    # Kill stale ADB server to prevent hangs on the first device command in this tier.
    Restart-AdbServer

    # Ensure emulator is running
    if (-not (Test-SingleEmulator)) {
        $emu1Ok = Start-SingleEmulator
        if ($emu1Ok) { $script:startedEmu1 = $true }
    } else {
        $emu1Ok = $true
    }

    if ($emu1Ok) {
        # Install APK and push game data via SHA256-indexed deps
        $emu1Serial = (Adb-Timeout -AdbArgs @("devices") -Seconds 5) |
            Select-String "(emulator-\d+)\s+device" |
            ForEach-Object { $_.Matches[0].Groups[1].Value } |
            Select-Object -First 1
        if ($emu1Serial) {
            Install-AppAndData -Serial $emu1Serial
        } else {
            Install-ApkOnDevice | Out-Null
            Push-GameDataToDevice
        }

        $singleEmuFailureStreak = 0
        foreach ($test in $tierSingleEmu) {
            if ($stopEarly) { break }
            $result = Invoke-SingleTest -Test $test
            if ($result.Status -eq "PASS") {
                $singleEmuFailureStreak = 0
                continue
            }

            $singleEmuFailureStreak++
            $needsRecovery = Test-SingleEmulatorFailureNeedsRecovery -Result $result
            if ($needsRecovery -or $singleEmuFailureStreak -ge 3) {
                $reason = if ($needsRecovery) {
                    "launcher-health failure signature after $($result.Name)"
                } else {
                    "$singleEmuFailureStreak consecutive single-emulator failures"
                }
                Write-Status "Tier 2 recovery: $reason" "Yellow"
                if (Recover-SingleEmulatorEnvironment -SerialRef ([ref]$emu1Serial)) {
                    $singleEmuFailureStreak = 0
                } else {
                    Write-Status "Tier 2 recovery failed; stopping remaining single-emulator tests" "Red"
                    break
                }
            }
        }
    } else {
        foreach ($test in $tierSingleEmu) {
            $infraSkipped += @{ Name = $test.Name; Reason = "could not start emulator"; Type = $test.Type }
        }
    }
}

# ── Tier 3: extract regression tests ─────────────────────────────────────

if ($tierExtract.Count -gt 0 -and -not $stopEarly) {
    Write-Host ""
    Write-Host "== Tier 3: Extract regression tests ==" -ForegroundColor Cyan

    $hasGameData = Test-GameDataAvailable
    if (-not $hasGameData) {
        foreach ($test in $tierExtract) {
            $infraSkipped += @{ Name = $test.Name; Reason = "no game data (CD images)"; Type = $test.Type }
        }
    } else {
        # Ensure emulator is running (may already be from tier 2)
        if (-not (Test-SingleEmulator)) {
            $emu1Ok = Start-SingleEmulator
            if ($emu1Ok) { $script:startedEmu1 = $true }
        } else {
            $emu1Ok = $true
        }

        if ($emu1Ok) {
            Install-ApkOnDevice | Out-Null
            Push-GameDataToDevice
            foreach ($test in $tierExtract) {
                if ($stopEarly) { break }
                $result = Invoke-SingleTest -Test $test
                if ($result.Status -ne "PASS") {
                    Write-Status "Tier 3 recovery: reprovisioning primary emulator after $($result.Name)" "Yellow"
                    if (-not (Recover-SingleEmulatorEnvironment -SerialRef ([ref]$emu1Serial))) {
                        Write-Status "Tier 3 recovery failed; stopping remaining extract tests" "Red"
                        break
                    }
                }
            }
        } else {
            foreach ($test in $tierExtract) {
                $infraSkipped += @{ Name = $test.Name; Reason = "could not start emulator"; Type = $test.Type }
            }
        }
    }
}

# ── Tier 4: dual-emulator tests ──────────────────────────────────────────

if ($tierDualEmu.Count -gt 0 -and -not $stopEarly) {
    Write-Host ""
    Write-Host "== Tier 4: Dual-emulator tests ==" -ForegroundColor Cyan

    # Ensure first emulator is running (may already be from earlier tiers)
    if (-not (Test-SingleEmulator)) {
        $emu1Ok = Start-SingleEmulator
        if ($emu1Ok) { $script:startedEmu1 = $true }
    } else {
        $emu1Ok = $true
    }

    # Ensure second emulator
    $emu2Ok = $false
    if ($emu1Ok) {
        if (-not (Test-TwoEmulators)) {
            $emu2Ok = Start-SecondEmulator
            if ($emu2Ok) { $script:startedEmu2 = $true }
        } else {
            $emu2Ok = $true
        }
    }

    # Ensure matchmaking server
    $serverOk = $false
    if ($emu2Ok) {
        if (-not (Test-MatchmakingServer)) {
            $script:autoServerProc = Start-MatchmakingServer
            $serverOk = ($null -ne $script:autoServerProc)
        } else {
            $serverOk = $true
        }
    }

    if ($emu2Ok -and $serverOk) {
        # Install APK + push data on the second emulator
        $devices = Adb-Timeout -AdbArgs @("devices") -Seconds 5
        $serials = [regex]::Matches($devices, "(emulator-\d+)\s+device") |
            ForEach-Object { $_.Groups[1].Value } | Sort-Object
        $emu1Serial = $null
        $emu2Serial = $null
        if ($serials.Count -ge 2) {
            $emu1Serial = $serials | Select-Object -First 1
            $emu2Serial = $serials | Select-Object -Last 1
            Install-AppAndData -Serial $emu2Serial
        }

        foreach ($test in $tierDualEmu) {
            if ($stopEarly) { break }
            $result = Invoke-SingleTest -Test $test
            if ($result.Status -ne "PASS") {
                Write-Status "Tier 4 recovery: recycling dual-emulator environment after $($result.Name)" "Yellow"
                if (-not (Recover-DualEmulatorEnvironment -PrimarySerialRef ([ref]$emu1Serial) -SecondarySerialRef ([ref]$emu2Serial))) {
                    Write-Status "Tier 4 recovery failed; stopping remaining dual-emulator tests" "Red"
                    break
                }
            }
        }
    } else {
        $reason = if (-not $emu1Ok) { "could not start emulator" }
        elseif (-not $emu2Ok) { "could not start second emulator" }
        else { "could not start matchmaking server" }
        foreach ($test in $tierDualEmu) {
            $infraSkipped += @{ Name = $test.Name; Reason = $reason; Type = $test.Type }
        }
    }
}

$totalSw.Stop()
$totalElapsed = $totalSw.Elapsed.ToString("hh\:mm\:ss")

# Merge manual + infra skips
$allSkipped = @($manualSkipped) + @($infraSkipped)

# -- Generate report --

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  TEST RESULTS" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# Console summary
foreach ($r in $results) {
    $icon = if ($r.Status -eq "PASS") { "+" } else { "!" }
    $color = if ($r.Status -eq "PASS") { "Green" } else { "Red" }
    Write-Host "  [$icon] $($r.Status.PadRight(7))  $($r.Elapsed)  $($r.Name)" -ForegroundColor $color
}
foreach ($s in $allSkipped) {
    Write-Host "  [-] SKIP           $($s.Name)  ($($s.Reason))" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "  Passed: $passCount  Failed: $failCount  Timeouts: $timeoutCount  Skipped: $($allSkipped.Count)  Total time: $totalElapsed"
Write-Host ""

# Markdown report
$md = @()
$md += "# Test Report - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$md += ""
$md += "## Summary"
$md += "- Passed: $passCount"
$md += "- Failed: $failCount"
$md += "- Timeouts: $timeoutCount"
$md += "- Skipped: $($allSkipped.Count)"
$md += "- Total time: $totalElapsed"
$md += "- Per-test timeout: ${TestTimeoutSeconds}s"
$md += "- Auto-provisioned: emu1=$($script:startedEmu1) emu2=$($script:startedEmu2) server=$(($null -ne $script:autoServerProc)) docker=$($script:startedDocker)"
$md += ""
$md += "## Results"
$md += ""
$md += "| Status | Time | Test | Type |"
$md += "|--------|------|------|------|"
foreach ($r in $results) {
    $md += "| $($r.Status) | $($r.Elapsed) | $($r.Name) | $($r.Type) |"
}
foreach ($s in $allSkipped) {
    $md += "| SKIP | -- | $($s.Name) | $($s.Type) ($($s.Reason)) |"
}
$md += ""

if ($failCount -gt 0 -or $timeoutCount -gt 0) {
    $md += "## Non-passing Results"
    $md += ""
    foreach ($r in ($results | Where-Object { $_.Status -eq "FAIL" -or $_.Status -eq "TIMEOUT" })) {
        $md += "### $($r.Name)"
        $md += "- Status: $($r.Status)"
        $md += "- Exit code: $($r.ExitCode)"
        $md += "- Log: ``$(Split-Path $r.LogFile -Leaf)``"
        if (Test-Path $r.LogFile) {
            $excerpt = Get-ReportLogExcerpt -LogFile $r.LogFile -Status $r.Status
            $md += '```'
            $md += $excerpt
            $md += '```'
        }
        $md += ""
    }
}

$md -join "`n" | Set-Content -Path $reportFile -Encoding utf8
Write-Host "  Report: $reportFile" -ForegroundColor Cyan
Write-Host ""

# -- Cleanup auto-provisioned infrastructure (reverse order) --

if ($script:startedDocker) {
    Write-Host "Stopping Docker NAT containers..." -ForegroundColor Yellow
    Stop-DockerNat
}

# Note: we do NOT stop emulators or matchmaking servers -- they're useful for
# by-hand testing after the run. Each MP test kills/restarts the server as
# needed in its own Phase 1.

if ($failCount -gt 0) { exit 1 } else { exit 0 }
