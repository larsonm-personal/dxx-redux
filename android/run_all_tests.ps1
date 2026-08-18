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
            7. Matchmaking server started for dual-emulator tests that request it
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

.PARAMETER FullExtracts
    Run every CD extraction regression spec. By default, run_all_tests samples one spec using a git-commit seed.

.PARAMETER ExtractSampleCount
    Number of CD extraction specs to sample when -FullExtracts is not set.

.PARAMETER ExtendedGraphics
    Add expensive or fault-injected graphics coverage. This replays the
    complete input-demo corpus through the graphics binaries and includes the
    forced merged-wall fallback canary. The default profile uses fixed graphics
    canaries while the full demo corpus runs headlessly.

.PARAMETER ExtendedMultiplayer
    Run the 90-second sustained multiplayer connectivity soak after the normal
    two-player lobby, launch, and in-game assertions. The default profile keeps
    the complete multiplayer smoke but omits the soak hold.

.PARAMETER Target45Minutes
    Select the same randomized percentage from every infrastructure and script
    type group, using recent report timings to target approximately 45 minutes.

.PARAMETER SampleSeed
    Optional nonzero seed for reproducing a targeted sample.

.EXAMPLE
    .\run_all_tests.ps1
    .\run_all_tests.ps1 -Filter "test_death*"
    .\run_all_tests.ps1 -StopOnFail
    .\run_all_tests.ps1 -SkipDocker
    .\run_all_tests.ps1 -Target45Minutes
#>

param(
    [string]$Filter,
    [switch]$IncludeManual,
    [switch]$StopOnFail,
    [string]$ReportDir,
    [int]$TestTimeoutSeconds = 120,
    [switch]$SkipDocker,
    [switch]$FullExtracts,
    [int]$ExtractSampleCount = 1,
    [switch]$ExtendedGraphics,
    [switch]$ExtendedMultiplayer,
    [switch]$Target45Minutes,
    [ValidateRange(0, [int]::MaxValue)]
    [int]$SampleSeed = 0
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $PSCommandPath
$helpersDir = Join-Path $scriptDir "helpers"
$repoRoot = Split-Path $scriptDir

. "$helpersDir\test_helpers.ps1"
. "$helpersDir\test_suite_progress.ps1"
. "$helpersDir\runtime_targeted_sampling.ps1"
. (Join-Path (Join-Path $scriptDir "tests") "extract_regression_spec_helpers.ps1")
. (Join-Path (Join-Path $scriptDir "tests") "input_demo_host_build_guard.ps1")
. (Join-Path (Join-Path $scriptDir "tests") "input_demo_graphics_canary_helpers.ps1")

# -- Report directory --

if (-not $ReportDir) {
    $ReportDir = Join-Path $repoRoot "temp\test_reports"
}
New-Item -Path $ReportDir -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$reportFile = Join-Path $ReportDir "report_$timestamp.md"
$script:cancelRequested = $false
$script:cancelCleanupStarted = $false

function Stop-ChildProcessTree {
    param([int]$ParentProcessId)

    $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$ParentProcessId" -ErrorAction SilentlyContinue)
    foreach ($child in $children) {
        Stop-ChildProcessTree -ParentProcessId ([int]$child.ProcessId)
        try {
            Stop-Process -Id ([int]$child.ProcessId) -Force -ErrorAction SilentlyContinue
        } catch {}
    }
}

function Request-TestSuiteCancel {
    if ($script:cancelCleanupStarted) {
        return
    }
    $script:cancelRequested = $true
    $script:cancelCleanupStarted = $true
    Write-Host ""
    Write-Host "Ctrl-C received; stopping test runner children" -ForegroundColor Yellow
    Stop-ChildProcessTree -ParentProcessId $PID
}

$script:cancelHandler =
[System.ConsoleCancelEventHandler] {
    param($sender, $eventArgs)
    $eventArgs.Cancel = $true
    Request-TestSuiteCancel
    [Environment]::Exit(130)
}
[Console]::add_CancelKeyPress($script:cancelHandler)

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
    return (Ensure-ExtractRegressionOracles -RepoRoot $repoRoot -Context "Full test suite extract tier")
}

function Test-DockerAvailable {
    if ($SkipDocker) { return $false }
    try {
        $null = docker version --format '{{.Server.Version}}' 2>&1
        return ($LASTEXITCODE -eq 0)
    } catch { return $false }
}

function Test-AnyCommandAvailable {
    param([string[]]$Names)

    foreach ($name in $Names) {
        if (Get-Command $name -ErrorAction SilentlyContinue) {
            return $true
        }
    }
    return $false
}

function Get-RegressionDemoGameCounts {
    $demoRoot = Join-Path $scriptDir "regression_demos"
    $counts = @{ all = 0; d1 = 0; d2 = 0 }
    if (-not (Test-Path -LiteralPath $demoRoot)) {
        return $counts
    }

    $demos = @(Get-ChildItem -Path $demoRoot -Recurse -Filter "*.dximdemo" -File -ErrorAction SilentlyContinue)
    foreach ($demo in $demos) {
        $counts['all']++
        if ($demo.BaseName -like "d1_*") {
            $counts['d1']++
        } elseif ($demo.BaseName -like "d2_*") {
            $counts['d2']++
        }
    }
    return $counts
}

function Get-RegressionDemoCount {
    param(
        [ValidateSet('all', 'd1', 'd2')]
        [string]$RecordedGame = 'all'
    )

    $counts = Get-RegressionDemoGameCounts
    return $counts[$RecordedGame]
}

function Test-InputDemoCorpusAvailable {
    param(
        [ValidateSet('all', 'd1', 'd2')]
        [string]$RecordedGame = 'all'
    )

    return (Get-RegressionDemoCount -RecordedGame $RecordedGame) -gt 0
}

function Test-InputDemoDeterminismFixturesAvailable {
    $fixtureListPath = Join-Path $scriptDir "tests/input_demo_determinism_fixtures.txt"
    if (-not (Test-Path -LiteralPath $fixtureListPath -PathType Leaf)) {
        return $false
    }

    foreach ($line in [System.IO.File]::ReadLines($fixtureListPath)) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith("#")) {
            continue
        }
        $parts = $trimmed -split '\|', 2
        if ($parts.Count -ne 2) {
            return $false
        }
        $path = $parts[1].Trim()
        $resolvedPath = if ([System.IO.Path]::IsPathRooted($path)) {
            [System.IO.Path]::GetFullPath($path)
        } else {
            [System.IO.Path]::GetFullPath((Join-Path $repoRoot $path))
        }
        if (-not (Test-Path -LiteralPath $resolvedPath -PathType Leaf)) {
            return $false
        }
    }

    return $true
}

function Get-TestBaseName {
    param([string]$TestName)

    if ($TestName -like "test_input_demo_regressions*") {
        return "test_input_demo_regressions"
    }

    switch ($TestName) {
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

function Get-GitCommitSeed {
    try {
        $commit = git -C $repoRoot rev-parse --verify HEAD 2>$null
        if ($LASTEXITCODE -eq 0 -and $commit) {
            $commitText = ($commit | Select-Object -First 1).Trim()
            $prefix = $commitText.Substring(0, [Math]::Min(8, $commitText.Length))
            return [int]([Convert]::ToUInt32($prefix, 16) -band 0x7fffffff)
        }
    } catch {}
    return 1
}

$extractSampleSeed = Get-GitCommitSeed
if (-not $FullExtracts -and $ExtractSampleCount -lt 1) {
    Write-Host "FAIL: -ExtractSampleCount must be at least 1 unless -FullExtracts is set" -ForegroundColor Red
    exit 1
}
$testAllExtractsTimeout = if ($FullExtracts) { 7200 } else { [Math]::Max(600, 300 * $ExtractSampleCount) }

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
$tierServerManagedDualEmuTests = @()

# Per-test timeout overrides (seconds) for multi-phase tests
$testTimeouts = @{
    "test_autoselect_crash_unified"       = 240
    "test_keyboard_defaults"              = 240
    "test_engine_prefs_unified"           = 240
    "test_gog_installer_d1_unified"       = 420
    "test_gog_installer_redbook_unified"  = 420
    "test_gradle_unit_tests"              = 600
    "test_input_demo_determinism_matrix"  = 600
    "test_input_demo_regressions"         = 900
    "test_input_demo_regressions_graphics" = 900
    "test_level_metadata_benchmark"       = 180
    "test_mission_zip_batch"              = 3600
    "test_mod_loading"                    = 360
    "test_saf_archiver"                   = 360
    "test_mac_extract_saf"                = 360
    "test_saf_redbook"                    = 420
    "test_all_extracts"                   = $testAllExtractsTimeout
    "test_native_host_unit_tests"         = 1200
    "test_mp"                             = 240
    "test_lan"                            = 240
    "test_server_integration"             = 600
    "test_secret_area_baseline_diff"      = 60
    "test_test_helpers_process_wait"      = 60
    "test_validate_extract_regression_specs" = 60
    "test_validate_automation_catalog"    = 60
}

function Get-RunTestJson5ChildTimeoutSeconds {
    param(
        [string]$ScriptPath,
        [string]$TestName
    )

    $scriptTimeout = [Math]::Max(
        [Math]::Max(300, $TestTimeoutSeconds),
        (Get-ScriptTimeoutSeconds -ScriptPath $ScriptPath)
    )
    if ((Get-ScriptIsLauncher -ScriptPath $ScriptPath) -and $scriptTimeout -lt 600) {
        $scriptTimeout = 600
    }
    if ($testTimeouts.ContainsKey($TestName)) {
        $scriptTimeout = [Math]::Max($scriptTimeout, [int]$testTimeouts[$TestName])
    }
    return [int]$scriptTimeout
}

function Get-RunAllJson5TimeoutSeconds {
    param(
        [string]$ScriptPath,
        [int]$ChildTimeoutSeconds
    )

    $gameCount = @(Get-ScriptGameInfo -ScriptPath $ScriptPath).Count
    if ($gameCount -lt 1) {
        $gameCount = 1
    }
    $perGameOverheadSeconds = if (Get-ScriptIsLauncher -ScriptPath $ScriptPath) { 90 } else { 45 }
    return [int](($ChildTimeoutSeconds + $perGameOverheadSeconds) * $gameCount + 30)
}

$extractTests = @(
    "test_all_extracts",
    "test_mac_extract_saf",
    "test_gog_installer_d1_unified",
    "test_gog_installer_redbook_unified"
)  # single emulator + game data, run before the dual-emulator tier
$noInfraTests = @(
    "test_cue_iso",
    "test_fpcalc_and_acoustid",
    "test_game_data_asset_manifest_writer",
    "test_gradle_unit_tests",
    "test_input_demo_determinism_matrix",
    "test_input_demo_regressions",
    "test_input_demo_regressions_graphics",
    "test_input_demo_runtime_smoke",
    "test_level_metadata_benchmark",
    "test_native_host_unit_tests",
    "test_server_integration",
    "test_secret_area_baseline_diff",
    "test_input_demo_state_trace_compare",
    "test_input_demo_rng_trace_compare",
    "test_test_report_runtimes",
    "test_test_helpers_process_wait",
    "test_test_suite_progress",
    "test_validate_extract_regression_specs",
    "test_validate_automation_catalog"
)

$allTests = @()
$supportScripts = @()
$catalogErrors = @()
$testsDir = Join-Path $scriptDir "tests"
$ps1Files = @(Get-ChildItem -Path $testsDir -Filter "test_*.ps1" -File -ErrorAction SilentlyContinue | Sort-Object Name)
$ps1TestNames = @{}
$ps1SupportOwners = @{}
foreach ($ps1File in $ps1Files) {
    $ps1TestNames[$ps1File.BaseName] = $true
    $supportOwner = Get-PowerShellTestSupportOwner -ScriptPath $ps1File.FullName
    if ($supportOwner) {
        $ps1SupportOwners[$ps1File.BaseName] = $supportOwner
    }
}
foreach ($supportName in $ps1SupportOwners.Keys) {
    $supportOwner = $ps1SupportOwners[$supportName]
    if ($supportName -eq $supportOwner) {
        $catalogErrors += "$supportName.ps1: support script cannot own itself"
    } elseif (-not $ps1TestNames.ContainsKey($supportOwner)) {
        $catalogErrors += "$supportName.ps1: owner '$supportOwner' is not a PowerShell test"
    } elseif ($ps1SupportOwners.ContainsKey($supportOwner)) {
        $catalogErrors += "$supportName.ps1: owner '$supportOwner' is itself a support script"
    }
}

# json5 game-automation scripts (run via run_test.ps1)
$gameScriptsDir = Join-Path $scriptDir "game_scripts"
$json5Files = @(Get-ChildItem -Path $gameScriptsDir -Filter "test_*.json5" -File -ErrorAction SilentlyContinue | Sort-Object Name)
foreach ($t in $json5Files) {
    $info = Get-TestScriptInfo -ScriptPath $t.FullName
    if ($null -eq $info) {
        $catalogErrors += "$($t.Name): missing or unparseable first _info object"
        continue
    }
    $standalone = -not ($info._standalone -eq $false)
    $owner = if ($info._owner) { [string]$info._owner } else { $null }
    if (-not $standalone) {
        if (-not $owner) {
            $catalogErrors += "$($t.Name): _standalone=false requires _owner"
        } elseif (-not $ps1TestNames.ContainsKey($owner)) {
            $catalogErrors += "$($t.Name): owner '$owner' is not a top-level PowerShell test"
        } else {
            $supportScripts += @{ Name = $t.BaseName; Owner = $owner; Type = "json5" }
        }
        continue
    }
    if ($owner) {
        $catalogErrors += "$($t.Name): standalone scripts cannot declare _owner"
    }
    if ($ps1TestNames.ContainsKey($t.BaseName)) {
        $catalogErrors += "$($t.Name): duplicate top-level test name '$($t.BaseName)'"
    }
    $name = $t.BaseName
    $childTimeoutSeconds = Get-RunTestJson5ChildTimeoutSeconds -ScriptPath $t.FullName -TestName $name
    $timeoutSeconds = Get-RunAllJson5TimeoutSeconds -ScriptPath $t.FullName -ChildTimeoutSeconds $childTimeoutSeconds
    $allTests += @{
        Name = $name
        BaseName = $name
        Type = "json5"
        Path = $t.FullName
        Requires = "emulator"
        TimeoutSeconds = $timeoutSeconds
        Arguments = @("-TimeoutSeconds", $childTimeoutSeconds.ToString())
    }
}

if ($catalogErrors.Count -gt 0) {
    Write-Host "FAIL: Invalid automation test catalog" -ForegroundColor Red
    foreach ($catalogError in $catalogErrors) {
        Write-Host "  $catalogError" -ForegroundColor Red
    }
    exit 1
}

# ps1 integration tests
foreach ($t in $ps1Files) {
    $name = $t.BaseName
    if ($ps1SupportOwners.ContainsKey($name)) {
        $supportScripts += @{ Name = $name; Owner = $ps1SupportOwners[$name]; Type = "ps1" }
        continue
    }
    if ($name -in @("test_input_demo_regressions", "test_input_demo_regressions_graphics")) {
        continue
    }
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

    $entry = @{
        Name = $name
        BaseName = $baseName
        Type = "ps1"
        Path = $t.FullName
        Requires = $req
        NeedsTierServer = ($name -in $tierServerManagedDualEmuTests)
        TimeoutSeconds = $timeoutSeconds
        DemoRunMode = if ($baseName -eq "test_input_demo_regressions") {
            if ($name -eq "test_input_demo_regressions_graphics") { "graphics" } else { "headless" }
        } else {
            ""
        }
    }
    if ($name -eq "test_all_extracts") {
        if ($FullExtracts) {
            $entry.Arguments = @("-All")
        } else {
            $entry.Arguments = @("-SampleCount", $ExtractSampleCount.ToString(), "-Seed", $extractSampleSeed.ToString())
        }
    } elseif ($name -eq "test_mac_extract_saf") {
        $entry.Arguments = @("-SkipBuild")
    } elseif ($name -eq "test_mp") {
        $soakSeconds = if ($ExtendedMultiplayer) { 90 } else { 0 }
        $entry.Arguments = @("-SoakSeconds", $soakSeconds.ToString())
    }
    $allTests += $entry
}

$inputDemoRegressionHeadlessWrapper = Join-Path $testsDir "test_input_demo_regressions.ps1"
$inputDemoRegressionGraphicsWrapper = Join-Path $testsDir "test_input_demo_regressions_graphics.ps1"
$inputDemoCanaryManifest = Read-InputDemoGraphicsCanaryManifest -ManifestPath (Join-Path $testsDir "input_demo_graphics_canaries.txt")
$d1GraphicsCanary = $inputDemoCanaryManifest['d1']
$d2GraphicsCanary = $inputDemoCanaryManifest['d2']
$inputDemoPrimaryRoot = Join-Path $repoRoot "temp\input_demo_primary_results_$timestamp"
$d1PrimaryResultRoot = Join-Path $inputDemoPrimaryRoot "d1"
$d2PrimaryResultRoot = Join-Path $inputDemoPrimaryRoot "d2"
$d1InD2PrimaryResultRoot = Join-Path $inputDemoPrimaryRoot "d1-in-d2"
$inputDemoRegressionMatrix = @(
    @{
        Name = "test_input_demo_regressions_d1"
        Section = "d1/headless"
        RunMode = "headless"
        RecordedGame = "d1"
        Path = $inputDemoRegressionHeadlessWrapper
        Arguments = @("-RecordedGame", "d1", "-Game", "d1", "-RunMode", "headless", "-ResultArchiveRoot", $d1PrimaryResultRoot)
    },
    @{
        Name = "test_input_demo_regressions_d2"
        Section = "d2/headless"
        RunMode = "headless"
        RecordedGame = "d2"
        Path = $inputDemoRegressionHeadlessWrapper
        Arguments = @("-RecordedGame", "d2", "-Game", "d2", "-RunMode", "headless", "-ResultArchiveRoot", $d2PrimaryResultRoot)
    },
    @{
        Name = "test_input_demo_regressions_d1_in_d2"
        Section = "d1-in-d2/headless"
        RunMode = "headless"
        RecordedGame = "d1"
        Path = $inputDemoRegressionHeadlessWrapper
        Arguments = @("-RecordedGame", "d1", "-Game", "d2", "-RunMode", "headless", "-D1InD2", "-ResultArchiveRoot", $d1InD2PrimaryResultRoot)
    }
)
if ($ExtendedGraphics) {
    $inputDemoRegressionMatrix += @(
        @{
            Name = "test_input_demo_regressions_d1_graphics"
            Section = "d1/graphics-full"
            RunMode = "graphics"
            RecordedGame = "d1"
            Path = $inputDemoRegressionGraphicsWrapper
            Arguments = @("-RecordedGame", "d1", "-Game", "d1", "-ReferenceResultRoot", $d1PrimaryResultRoot)
        },
        @{
            Name = "test_input_demo_regressions_d2_graphics"
            Section = "d2/graphics-full"
            RunMode = "graphics"
            RecordedGame = "d2"
            Path = $inputDemoRegressionGraphicsWrapper
            Arguments = @("-RecordedGame", "d2", "-Game", "d2", "-ReferenceResultRoot", $d2PrimaryResultRoot)
        },
        @{
            Name = "test_input_demo_regressions_d1_in_d2_graphics"
            Section = "d1-in-d2/graphics-full"
            RunMode = "graphics"
            RecordedGame = "d1"
            Path = $inputDemoRegressionGraphicsWrapper
            Arguments = @("-RecordedGame", "d1", "-Game", "d2", "-D1InD2", "-ReferenceResultRoot", $d1InD2PrimaryResultRoot)
        }
    )
} else {
    $inputDemoRegressionMatrix += @(
        @{
            Name = "test_input_demo_regressions_d1_graphics_canary"
            Section = "d1/graphics-canary"
            RunMode = "graphics"
            RecordedGame = "d1"
            DemoFileName = $d1GraphicsCanary.FileName
            DemoSha256 = $d1GraphicsCanary.Sha256
            DemoCount = 1
            Path = $inputDemoRegressionGraphicsWrapper
            Arguments = @("-RecordedGame", "d1", "-Game", "d1", "-DemoFileName", $d1GraphicsCanary.FileName, "-ReferenceResultRoot", $d1PrimaryResultRoot)
        },
        @{
            Name = "test_input_demo_regressions_d2_graphics_canary"
            Section = "d2/graphics-canary"
            RunMode = "graphics"
            RecordedGame = "d2"
            DemoFileName = $d2GraphicsCanary.FileName
            DemoSha256 = $d2GraphicsCanary.Sha256
            DemoCount = 1
            Path = $inputDemoRegressionGraphicsWrapper
            Arguments = @("-RecordedGame", "d2", "-Game", "d2", "-DemoFileName", $d2GraphicsCanary.FileName, "-ReferenceResultRoot", $d2PrimaryResultRoot)
        },
        @{
            Name = "test_input_demo_regressions_d1_in_d2_graphics_canary"
            Section = "d1-in-d2/graphics-canary"
            RunMode = "graphics"
            RecordedGame = "d1"
            DemoFileName = $d1GraphicsCanary.FileName
            DemoSha256 = $d1GraphicsCanary.Sha256
            DemoCount = 1
            Path = $inputDemoRegressionGraphicsWrapper
            Arguments = @("-RecordedGame", "d1", "-Game", "d2", "-D1InD2", "-DemoFileName", $d1GraphicsCanary.FileName, "-ReferenceResultRoot", $d1InD2PrimaryResultRoot)
        }
    )
}
foreach ($definition in $inputDemoRegressionMatrix) {
    if (-not (Test-Path -LiteralPath $definition.Path -PathType Leaf)) {
        continue
    }
    $entry = @{
        Name = $definition.Name
        BaseName = "test_input_demo_regressions"
        Type = "ps1"
        Path = $definition.Path
        Requires = "none"
        NeedsTierServer = $false
        TimeoutSeconds = $testTimeouts["test_input_demo_regressions"]
        DemoRunMode = $definition.RunMode
        DemoRecordedGame = $definition.RecordedGame
        DemoSection = $definition.Section
        Arguments = $definition.Arguments
    }
    if ($definition.ContainsKey("DemoFileName")) {
        $entry.DemoFileName = $definition.DemoFileName
        $entry.DemoSha256 = $definition.DemoSha256
    }
    if ($definition.ContainsKey("DemoCount")) {
        $entry.DemoCount = $definition.DemoCount
    }
    $allTests += $entry
}

# Apply filter
if ($Filter) {
    $allTests = @($allTests | Where-Object { Test-MatchesRequestedFilter -Test $_ -RequestedFilter $Filter })
}

$extendedGraphicsTests = @("test_merged_wall_two_pass_probe")
$profileSkipped = @()
if (-not $ExtendedGraphics) {
    $allTests = @($allTests | Where-Object {
            if ($_.Name -in $extendedGraphicsTests) {
                $profileSkipped += @{
                    Name = $_.Name
                    Reason = "requires -ExtendedGraphics"
                    Type = $_.Type
                }
                return $false
            }
            return $true
        })
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

$fixtureSkipped = @()
$inputDemoDeterminismFixturesAvailable = Test-InputDemoDeterminismFixturesAvailable
$runnableTests = @($runnableTests | Where-Object {
        $keep = $true
        if ($_.BaseName -eq "test_input_demo_regressions") {
            $recordedGame = if ($_.ContainsKey("DemoRecordedGame")) { $_["DemoRecordedGame"] } else { "all" }
            if (-not (Test-InputDemoCorpusAvailable -RecordedGame $recordedGame)) {
                $fixtureSkipped += @{ Name = $_.Name; Reason = "no input-demo $recordedGame regression fixtures"; Type = $_.Type }
                $keep = $false
            } elseif ($_.ContainsKey("DemoFileName")) {
                $canaryIssue = Get-InputDemoGraphicsCanaryIssue `
                    -DemoRoot (Join-Path $scriptDir "regression_demos") `
                    -Entry ([pscustomobject]@{ FileName = $_.DemoFileName; Sha256 = $_.DemoSha256 })
                if ($canaryIssue) {
                    $fixtureSkipped += @{ Name = $_.Name; Reason = "input-demo canary unavailable: $canaryIssue"; Type = $_.Type }
                    $keep = $false
                }
            }
        } elseif ($_.Name -eq "test_input_demo_determinism_matrix" -and -not $inputDemoDeterminismFixturesAvailable) {
            $fixtureSkipped += @{ Name = $_.Name; Reason = "input-demo determinism fixtures unavailable"; Type = $_.Type }
            $keep = $false
        }
        $keep
    })

function Sort-TestsForExecution {
    param([object[]]$Tests)

    return , @($Tests | Sort-Object Name)
}

# Group by infrastructure tier
$tierNone = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "none" })
$tierServer = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "server" })
$tierSingleEmu = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "emulator" })
$tierDualEmu = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "two_emulators" })
$tierExtract = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq "extract" })
$executionTests = @($tierNone) + @($tierServer) + @($tierSingleEmu) + @($tierExtract) + @($tierDualEmu)

$historicalRuntimeByName = @{}
$historicalReportPath = ""
$historicalReportCount = 0
try {
    $runtimeReader = Join-Path $helpersDir "get-test-report-runtimes.ps1"
    foreach ($record in @(& $runtimeReader -ReportDir $ReportDir -ExcludeReportPath $reportFile)) {
        $historicalRuntimeByName[$record.Name] = [Math]::Max(1, [int]$record.Seconds)
        $historicalReportPath = $record.ReportPath
        $historicalReportCount = $record.SourceReportCount
    }
} catch {
    Write-Host "WARN: Could not read historical test runtimes: $_" -ForegroundColor Yellow
}

$knownSelectedRuntimes = @(
    $executionTests |
        Where-Object { $historicalRuntimeByName.ContainsKey($_.Name) } |
        ForEach-Object { [int]$historicalRuntimeByName[$_.Name] } |
        Sort-Object
)
$fallbackRuntime = if ($Target45Minutes) { 60 } else { 1 }
if ($knownSelectedRuntimes.Count -gt 0) {
    $middle = [int][Math]::Floor($knownSelectedRuntimes.Count / 2)
    $fallbackRuntime = if (($knownSelectedRuntimes.Count % 2) -eq 0) {
        [int][Math]::Max(1, [Math]::Round(
                ($knownSelectedRuntimes[$middle - 1] + $knownSelectedRuntimes[$middle]) / 2,
                [MidpointRounding]::AwayFromZero
            ))
    } else {
        $knownSelectedRuntimes[$middle]
    }
}

for ($index = 0; $index -lt $executionTests.Count; $index++) {
    $test = $executionTests[$index]
    $estimatedRuntime = if ($historicalRuntimeByName.ContainsKey($test.Name)) {
        [int]$historicalRuntimeByName[$test.Name]
    } else {
        $fallbackRuntime
    }
    $test["ProgressIndex"] = $index + 1
    $test["EstimatedRuntime"] = $estimatedRuntime
}

$runtimeSample = $null
if ($Target45Minutes -and $executionTests.Count -gt 0) {
    $runtimeSample = Select-RuntimeProportionalItems -Items $executionTests -TargetSeconds 2700 `
        -GroupProperties Requires, Type -Seed $SampleSeed
    $sampledTests = @($runtimeSample.Items)
    $sampledNames = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($test in $sampledTests) { [void]$sampledNames.Add([string]$test.Name) }

    # Graphics comparisons consume the matching headless result from this run.
    foreach ($test in @($sampledTests | Where-Object { $_.Name -match '_graphics(?:_canary)?$' })) {
        $headlessName = $test.Name -replace '_graphics(?:_canary)?$', ''
        $headless = $executionTests | Where-Object Name -eq $headlessName | Select-Object -First 1
        if ($headless -and $sampledNames.Add([string]$headless.Name)) {
            $sampledTests += $headless
            $runtimeSample.EstimatedSeconds += [int]$headless.EstimatedRuntime
        }
    }
    foreach ($test in $executionTests) {
        if (-not $sampledNames.Contains([string]$test.Name)) {
            $profileSkipped += @{ Name = $test.Name; Reason = 'not selected by 45-minute sample'; Type = $test.Type }
        }
    }
    $runnableTests = @($sampledTests)
    $tierNone = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq 'none' })
    $tierServer = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq 'server' })
    $tierSingleEmu = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq 'emulator' })
    $tierDualEmu = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq 'two_emulators' })
    $tierExtract = Sort-TestsForExecution @($runnableTests | Where-Object { $_.Requires -eq 'extract' })
    $executionTests = @($tierNone) + @($tierServer) + @($tierSingleEmu) + @($tierExtract) + @($tierDualEmu)
    for ($index = 0; $index -lt $executionTests.Count; $index++) {
        $executionTests[$index]['ProgressIndex'] = $index + 1
    }
}
$selectedRegressionDemoTests = @($runnableTests | Where-Object { $_.BaseName -eq "test_input_demo_regressions" })
$selectedRegressionDemoSections = @($selectedRegressionDemoTests | ForEach-Object { $_.DemoSection } | Where-Object { $_ } | Sort-Object -Unique)
$regressionDemoCounts = Get-RegressionDemoGameCounts
$selectedRegressionDemoReplayCount = 0
foreach ($test in $selectedRegressionDemoTests) {
    if ($test.ContainsKey("DemoCount")) {
        $selectedRegressionDemoReplayCount += [int]$test.DemoCount
    } else {
        $recordedGame = if ($test.ContainsKey("DemoRecordedGame")) { $test["DemoRecordedGame"] } else { "all" }
        $selectedRegressionDemoReplayCount += Get-RegressionDemoCount -RecordedGame $recordedGame
    }
}

function Test-HostToolPrerequisites {
    if (Test-RegressionWindowsHost) {
        return $true
    }

    $hostToolTests = @(
        "test_cue_iso",
        "test_fpcalc_and_acoustid",
        "test_input_demo_determinism_matrix",
        "test_input_demo_regressions",
        "test_input_demo_regressions_graphics",
        "test_input_demo_runtime_smoke",
        "test_native_host_unit_tests",
        "test_server_integration"
    )
    $needsNativeHostTools = @($runnableTests | Where-Object {
            $_.Name -in $hostToolTests -or
            $_.BaseName -in $hostToolTests -or
            $_.Requires -in @("server", "two_emulators")
        }).Count -gt 0

    if (-not $needsNativeHostTools) {
        return $true
    }

    $missing = @()
    foreach ($tool in @("cc", "c++", "make", "cmake", "ctest", "cargo", "rustc")) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            $missing += $tool
        }
    }
    if (-not (Test-AnyCommandAvailable -Names @("ninja", "ninja-build"))) {
        $missing += "ninja"
    }
    if (-not (Test-AnyCommandAvailable -Names @("pkg-config", "pkgconf"))) {
        $missing += "pkg-config"
    }
    $pkgConfig = Get-Command pkg-config -ErrorAction SilentlyContinue
    if (-not $pkgConfig) {
        $pkgConfig = Get-Command pkgconf -ErrorAction SilentlyContinue
    }
    if ($pkgConfig) {
        foreach ($module in @("physfs", "sdl", "SDL_mixer", "libpng", "glew")) {
            & $pkgConfig.Source --exists $module 2>$null
            if ($LASTEXITCODE -ne 0) {
                $missing += "pkg-config:$module"
            }
        }
    }

    if ($missing.Count -eq 0) {
        return $true
    }

    Write-Host ""
    Write-Host "FAIL: Selected tests need Linux host build tools that are not on PATH" -ForegroundColor Red
    Write-Host "Missing: $($missing -join ', ')" -ForegroundColor Red
    Write-Host "Run from a terminal with sudo:" -ForegroundColor Yellow
    Write-Host "  ./android/get_deps/helpers/get_linux_build_prereqs.sh" -ForegroundColor Yellow
    return $false
}

function Restart-AdbServer {
    Write-Status "Restarting ADB server..." "DarkGray"
    Get-Process adb -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & $script:ADB start-server 2>&1 | Out-Null
    $ErrorActionPreference = $prevEAP
    Start-Sleep -Seconds 2
}

function Invoke-AutomaticStaleEmulatorCleanup {
    $cleanupScript = Join-Path $helpersDir "kill-stale-emulators.ps1"
    if (-not (Test-Path -LiteralPath $cleanupScript)) {
        return
    }

    Write-Status "Checking for stale emulator background state..." "DarkGray"
    & $cleanupScript -Kill
    $cleanupExit = $LASTEXITCODE
    if ($cleanupExit -eq 2) {
        Write-Status "Stale emulator state was cleaned before preflight" "Yellow"
    } elseif ($cleanupExit -ne 0) {
        Write-Status "Stale emulator cleanup did not complete cleanly (exit $cleanupExit), continuing with normal recovery" "Yellow"
    }
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

function Stop-TestSuiteEmulators {
    $onlineSerials = @(Get-OnlineEmulatorSerials)
    if ($onlineSerials.Count -eq 0) {
        return
    }

    $targetSerials = @()
    if ($script:startedEmu1 -and $onlineSerials -contains $script:PRIMARY_EMULATOR_SERIAL) {
        $targetSerials += $script:PRIMARY_EMULATOR_SERIAL
    }
    if ($script:startedEmu2 -and $onlineSerials -contains $script:SECONDARY_EMULATOR_SERIAL) {
        $targetSerials += $script:SECONDARY_EMULATOR_SERIAL
    }

    if ($targetSerials.Count -eq 0) {
        return
    }

    Write-Host "Stopping Android emulators..." -ForegroundColor Yellow
    foreach ($serial in $targetSerials) {
        Write-Status "Stopping emulator $serial" "Yellow"
        $null = Adb-Timeout -AdbArgs @("-s", $serial, "emu", "kill") -Seconds 10
    }

    Start-Sleep -Seconds 2
    $remaining = @(Get-OnlineEmulatorSerials | Where-Object { $targetSerials -contains $_ })
    if ($remaining.Count -gt 0) {
        Write-Status "Emulator shutdown did not complete for: $($remaining -join ', ')" "Yellow"
    }
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
    Invoke-AutomaticStaleEmulatorCleanup
    Restart-AdbServer

    $healthScript = Join-Path $helpersDir "emu_health.ps1"
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
        [ref]$SecondarySerialRef,
        [switch]$EnsureServer
    )

    Write-Status "Dual-emulator recovery: forcing clean emulator recycle and reprovisioning app/data" "Yellow"
    Invoke-AutomaticStaleEmulatorCleanup
    Restart-AdbServer

    if ($script:autoServerProc -and -not $script:autoServerProc.HasExited) {
        try { $script:autoServerProc.Kill() } catch {}
        try { $script:autoServerProc.WaitForExit(5000) } catch {}
        $script:autoServerProc = $null
    }

    $healthScript = Join-Path $helpersDir "emu_health.ps1"
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

    if ($EnsureServer -and -not (Test-MatchmakingServer)) {
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

    $healthScript = Join-Path $helpersDir "emu_health.ps1"

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

        $appDataOk = Install-AppAndData -Serial $serial
        if ($RequireStandardGameData -and -not $appDataOk) {
            Write-Status "Preflight: standard game data provisioning failed on $serial" "Red"
            return $null
        }
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
    $appDataOk = Install-AppAndData -Serial $serial
    if ($RequireStandardGameData -and -not $appDataOk) {
        Write-Status "Preflight: standard game data provisioning failed on $serial" "Red"
        return $null
    }
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
        if (-not (Test-EmulatorAccelerationAvailable)) {
            Write-Host "FAIL: Suite preflight cannot start an emulator without CPU acceleration" -ForegroundColor Red
            return $false
        }
        Invoke-AutomaticStaleEmulatorCleanup
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

$reportSidecarLogsByTestName = @{
    test_double_launch = "test_double_launch_result.txt"
    test_lan           = "lan_test_log.txt"
    test_mp            = "mp_test_log.txt"
}

function Add-ReportSidecarLog {
    param(
        [string]$TestName,
        [string]$LogFile,
        [datetime]$StartedAt
    )

    if (-not $reportSidecarLogsByTestName.ContainsKey($TestName)) {
        return $null
    }

    $sidecarLog = Join-Path (Join-Path $repoRoot "temp") $reportSidecarLogsByTestName[$TestName]
    if (-not (Test-Path -LiteralPath $sidecarLog -PathType Leaf)) {
        return $null
    }

    $sidecarItem = Get-Item -LiteralPath $sidecarLog -ErrorAction SilentlyContinue
    if (-not $sidecarItem -or $sidecarItem.Length -eq 0) {
        return $null
    }
    if ($StartedAt -and $sidecarItem.LastWriteTime -lt $StartedAt.AddSeconds(-2)) {
        return $null
    }

    $sidecarText = Get-Content -LiteralPath $sidecarLog -Raw -ErrorAction SilentlyContinue
    if (-not $sidecarText -or -not $sidecarText.Trim()) {
        return $null
    }

    $logDir = Split-Path -Parent $LogFile
    if ($logDir -and -not (Test-Path -LiteralPath $logDir -PathType Container)) {
        New-Item -Path $logDir -ItemType Directory -Force | Out-Null
    }

    "" | Out-File -FilePath $LogFile -Append -Encoding utf8
    "===== Sidecar log: $($sidecarItem.Name) =====" | Out-File -FilePath $LogFile -Append -Encoding utf8
    $sidecarText | Out-File -FilePath $LogFile -Append -Encoding utf8
    return $sidecarLog
}

function Get-PassingResultNotes {
    param([string]$LogFile)

    if (-not $LogFile -or -not (Test-Path -LiteralPath $LogFile -PathType Leaf)) {
        return @()
    }

    $notes = [System.Collections.Generic.List[string]]::new()
    $seen = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($line in @(Get-Content -LiteralPath $LogFile -ErrorAction SilentlyContinue)) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed -match 'support script \(not standalone\)') {
            continue
        }

        $note = $null
        if ($trimmed -cmatch 'Mission ZIP batch complete: .*?,\s*[1-9]\d*\s+skipped') {
            $note = "subtest skip summary: $trimmed"
        } elseif ($trimmed -cmatch 'SKIP \(') {
            $note = "internal skip: $trimmed"
        } elseif ($trimmed -cmatch '\bTIMEOUT:') {
            $note = "internal wait timeout: $trimmed"
        } elseif ($trimmed -cmatch '\bWARN(?:ING)?:') {
            $note = "warning: $trimmed"
        }

        if ($note -and $seen.Add($note)) {
            $notes.Add($note) | Out-Null
            if ($notes.Count -ge 4) {
                break
            }
        }
    }

    return @($notes.ToArray())
}

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  DXX-Redux Unattended Test Suite" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tests found: $($allTests.Count) selected, $($runnableTests.Count) runnable, $($manualSkipped.Count) manual-skipped, $($profileSkipped.Count) profile-skipped, $($supportScripts.Count) support-owned" -ForegroundColor White
Write-Host "  Tier 0 (no infra):       $($tierNone.Count)"
Write-Host "  Tier 1 (server only):    $($tierServer.Count)"
Write-Host "  Tier 2 (single emu):     $($tierSingleEmu.Count)"
Write-Host "  Tier 3 (extract):        $($tierExtract.Count)"
Write-Host "  Tier 4 (dual emu):       $($tierDualEmu.Count)"
if ($runtimeSample) {
    $sampleEstimate = Format-RunnerDurationEstimate -Seconds $runtimeSample.EstimatedSeconds
    Write-Host "  45-minute sample:       seed $($runtimeSample.Seed), $($executionTests.Count) tests, estimated $sampleEstimate"
    foreach ($group in $runtimeSample.Groups) {
        Write-Host ("    {0}: {1}/{2} ({3:P0}, before required dependencies)" -f `
                $group.Name, $group.Selected, $group.Available, $group.Fraction)
    }
}
$historicalTimingCount = @($executionTests | Where-Object { $historicalRuntimeByName.ContainsKey($_.Name) }).Count
if ($historicalReportPath) {
    Write-Host "  Historical timings:    $historicalTimingCount/$($executionTests.Count) from $historicalReportCount recent report(s), newest $(Split-Path $historicalReportPath -Leaf)"
} else {
    Write-Host "  Historical timings:      unavailable; using equal test weights"
}
$demoGraphicsProfile = if ($ExtendedGraphics) { "full graphics" } else { "graphics canaries" }
if ($selectedRegressionDemoSections.Count -gt 0) {
    Write-Host "  Demo regressions:      d1=$($regressionDemoCounts['d1']) d2=$($regressionDemoCounts['d2']) demo(s), sections: $($selectedRegressionDemoSections -join ', '), replay runs: $selectedRegressionDemoReplayCount"
    Write-Host "  Demo profile:          full headless corpus + $demoGraphicsProfile"
} else {
    Write-Host "  Demo regressions:       0 replay runs selected"
}
Write-Host ""

if ($runnableTests.Count -eq 0) {
    Write-Host "No runnable tests found" -ForegroundColor Yellow
}

if ($runnableTests.Count -gt 0 -and -not (Test-HostToolPrerequisites)) {
    exit 1
}

# -- Build APK if any emulator tests will run --

$needsApk = ($tierSingleEmu.Count + $tierDualEmu.Count + $tierExtract.Count) -gt 0
if ($runnableTests.Count -gt 0 -and $needsApk) {
    if (-not (Test-EmulatorAccelerationAvailable)) {
        Write-Host "FAIL: Selected tests require Android emulator CPU acceleration" -ForegroundColor Red
        exit 1
    }

    Write-Host "== Building debug APK ==" -ForegroundColor Cyan
    $gradleWrapper = Resolve-RegressionGradleWrapper -AndroidDir $scriptDir
    & $gradleWrapper -p $scriptDir assembleDebug --console=plain 2>&1 |
        Where-Object { $_ -match "^(> Task|BUILD |FAIL|error:|Execution failed|What went wrong|Exception)" } |
        ForEach-Object { Write-Host "  $_" }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL: APK build failed" -ForegroundColor Red
        exit 1
    }
    Write-Host "  Build OK" -ForegroundColor Green
    Write-Host ""
}

if ($runnableTests.Count -gt 0 -and -not (Invoke-SuitePreflight)) {
    exit 1
}

# -- Execution helpers --

$runTestScript = Join-Path $helpersDir "run_test.ps1"
$results = @()
$passCount = 0
$failCount = 0
$timeoutCount = 0
$skipCount = 0
$infraSkipped = @($profileSkipped) + @($fixtureSkipped)
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

    $remaining = Get-TestSuiteRemainingEstimate `
        -Tests $executionTests `
        -CurrentProgressIndex $Test.ProgressIndex
    $remainingEstimatedRuntime = $remaining.Seconds
    $remainingPercent = $remaining.Percent
    $remainingEstimate = Format-RunnerDurationEstimate -Seconds $remainingEstimatedRuntime
    $suiteElapsed = $totalSw.Elapsed.ToString("hh\:mm\:ss")
    Write-Host "  Test $($Test.ProgressIndex)/$($executionTests.Count), $suiteElapsed elapsed, estimated $remainingEstimate remaining ($remainingPercent%)" -ForegroundColor Cyan
    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  Running: $name  [$($Test.Type)]  (timeout: ${testTimeout}s)" -ForegroundColor White
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $startedAt = Get-Date

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
    $stdout = ""
    $stderr = ""
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
        if (-not ("$stdout$stderr").Trim()) {
            Add-ReportSidecarLog -TestName $name -LogFile $logFile -StartedAt $startedAt | Out-Null
        }
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

    $skipMarker = [regex]::Match("$stdout`n$stderr", '(?m)^RESULT:\s*SKIP(?:\s*\((?<reason>[^\r\n]*)\))?')
    $skipDeclared = $skipMarker.Success
    $status = Get-TestStatusFromExitCode -ExitCode $exitCode -TimedOut $timedOut -SkipDeclared $skipDeclared
    $color = switch ($status) {
        "PASS" { "Green" }
        "SKIP" { [void]($script:skipCount++); "DarkYellow" }
        "TIMEOUT" { [void]($script:timeoutCount++); "Yellow" }
        default { "Red" }
    }
    Write-Host "  $status ($elapsed)" -ForegroundColor $color

    if ($status -eq "PASS") {
        $script:passCount++
    } elseif ($status -eq "FAIL") {
        $script:failCount++
    }
    $notes = if ($status -eq "PASS") { @(Get-PassingResultNotes -LogFile $logFile) } else { @() }
    if ($notes.Count -gt 0) {
        Write-Host "    notes: $($notes.Count)" -ForegroundColor Yellow
    }

    $result = @{
        Name = $name; Type = $Test.Type; Status = $status
        ExitCode = $exitCode; Elapsed = $elapsed; LogFile = $logFile; Notes = $notes
        Reason = if ($status -eq "SKIP" -and $skipMarker.Groups["reason"].Success) {
            $skipMarker.Groups["reason"].Value
        } else { "" }
    }
    $script:results += $result

    if ($StopOnFail -and $status -in @("FAIL", "TIMEOUT")) {
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

    for ($index = 0; $index -lt $lines.Count; $index++) {
        if ($lines[$index] -match '^Structural diff:') {
            $end = [Math]::Min($lines.Count - 1, $index + 43)
            return @($lines[$index..$end])
        }
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
        Invoke-SingleTest -Test $test | Out-Null
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
            Invoke-SingleTest -Test $test | Out-Null
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
            if ($result.Status -in @("PASS", "SKIP")) {
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
            $emu1Serial = Get-OnlineEmulatorSerials | Select-Object -First 1
            Install-ApkOnDevice | Out-Null
            Push-GameDataToDevice
            foreach ($test in $tierExtract) {
                if ($stopEarly) { break }
                $result = Invoke-SingleTest -Test $test
                if ($result.Status -in @("FAIL", "TIMEOUT")) {
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
    $tier4NeedsServer = @($tierDualEmu | Where-Object { $_.NeedsTierServer }).Count -gt 0

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

    # Some dual-emulator tests own their server lifecycle; only provision a
    # tier-wide server for tests that explicitly request one.
    $serverOk = -not $tier4NeedsServer
    if ($emu2Ok) {
        if ($tier4NeedsServer -and -not (Test-MatchmakingServer)) {
            $script:autoServerProc = Start-MatchmakingServer
            $serverOk = ($null -ne $script:autoServerProc)
        } elseif ($tier4NeedsServer) {
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
            if ($result.Status -in @("FAIL", "TIMEOUT")) {
                Write-Status "Tier 4 recovery: recycling dual-emulator environment after $($result.Name)" "Yellow"
                if (-not (Recover-DualEmulatorEnvironment -PrimarySerialRef ([ref]$emu1Serial) -SecondarySerialRef ([ref]$emu2Serial) -EnsureServer:$tier4NeedsServer)) {
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
$accountedTestNames = [System.Collections.Generic.HashSet[string]]::new()
foreach ($result in $results) { [void]$accountedTestNames.Add([string]$result.Name) }
foreach ($skippedTest in $allSkipped) { [void]$accountedTestNames.Add([string]$skippedTest.Name) }
$notRun = @(
    $runnableTests |
        Where-Object { -not $accountedTestNames.Contains([string]$_.Name) } |
        ForEach-Object {
            @{
                Name = $_.Name
                Type = $_.Type
                Reason = if ($stopEarly) { "stopped after prior failure" } else { "execution did not reach this test" }
            }
        }
)

# -- Generate report --

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  TEST RESULTS" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

# Console summary
foreach ($r in $results) {
    $icon = if ($r.Status -eq "PASS") { "+" } elseif ($r.Status -eq "SKIP") { "-" } else { "!" }
    $color = if ($r.Status -eq "PASS") { "Green" } elseif ($r.Status -eq "SKIP") { "DarkYellow" } else { "Red" }
    $noteSuffix = if ($r.Notes -and $r.Notes.Count -gt 0) { "  notes=$($r.Notes.Count)" } else { "" }
    Write-Host "  [$icon] $($r.Status.PadRight(7))  $($r.Elapsed)  $($r.Name)$noteSuffix" -ForegroundColor $color
}
foreach ($s in $allSkipped) {
    Write-Host "  [-] SKIP           $($s.Name)  ($($s.Reason))" -ForegroundColor DarkGray
}
foreach ($item in $notRun) {
    Write-Host "  [ ] NOT_RUN        $($item.Name)  ($($item.Reason))" -ForegroundColor Yellow
}

Write-Host ""
$totalSkipped = $skipCount + $allSkipped.Count
Write-Host "  Passed: $passCount  Failed: $failCount  Timeouts: $timeoutCount  Skipped: $totalSkipped  Not run: $($notRun.Count)  Total time: $totalElapsed"
Write-Host ""

# Markdown report
$md = @()
$md += "# Test Report - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$md += ""
$md += "## Summary"
$md += "- Passed: $passCount"
$md += "- Failed: $failCount"
$md += "- Timeouts: $timeoutCount"
$md += "- Skipped: $totalSkipped"
$md += "- Not run: $($notRun.Count)"
$md += "- Total time: $totalElapsed"
$md += "- Per-test timeout: ${TestTimeoutSeconds}s"
if ($runtimeSample) {
    $md += "- 45-minute sample: seed $($runtimeSample.Seed), $($executionTests.Count) tests, estimated $(Format-RunnerDurationEstimate -Seconds $runtimeSample.EstimatedSeconds)"
    foreach ($group in $runtimeSample.Groups) {
        $md += ("  - {0}: {1}/{2} ({3:P0}, before required dependencies)" -f `
                $group.Name, $group.Selected, $group.Available, $group.Fraction)
    }
}
$md += "- Auto-provisioned: emu1=$($script:startedEmu1) emu2=$($script:startedEmu2) server=$(($null -ne $script:autoServerProc)) docker=$($script:startedDocker)"
if ($selectedRegressionDemoSections.Count -gt 0) {
    $md += "- Demo profile: full headless corpus + $demoGraphicsProfile; $selectedRegressionDemoReplayCount replay runs; graphics results compared with their primary headless result"
}
$md += ""
$md += "## Results"
$md += ""
$md += "| Status | Time | Test | Type |"
$md += "|--------|------|------|------|"
foreach ($r in $results) {
    $resultType = if ($r.Status -eq "SKIP" -and $r.Reason) { "$($r.Type) ($($r.Reason))" } else { $r.Type }
    $md += "| $($r.Status) | $($r.Elapsed) | $($r.Name) | $resultType |"
}
foreach ($s in $allSkipped) {
    $md += "| SKIP | -- | $($s.Name) | $($s.Type) ($($s.Reason)) |"
}
foreach ($item in $notRun) {
    $md += "| NOT_RUN | -- | $($item.Name) | $($item.Type) ($($item.Reason)) |"
}
$md += ""

$passingResultsWithNotes = @($results | Where-Object { $_.Status -eq "PASS" -and $_.Notes -and $_.Notes.Count -gt 0 })
if ($passingResultsWithNotes.Count -gt 0) {
    $md += "## Passing Results With Notes"
    $md += ""
    foreach ($r in $passingResultsWithNotes) {
        $md += "### $($r.Name)"
        $md += "- Log: ``$(Split-Path $r.LogFile -Leaf)``"
        foreach ($note in $r.Notes) {
            $md += "- $note"
        }
        $md += ""
    }
}

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

$retentionArtifacts = @(@($reportFile, $inputDemoPrimaryRoot) | Where-Object { Test-Path -LiteralPath $_ })
$retentionArtifacts += @(Get-ChildItem -LiteralPath $ReportDir -File | Where-Object { $_.Name -like "*_$timestamp.*" } | Select-Object -ExpandProperty FullName)
& (Join-Path $helpersDir "retain-recent-artifacts.ps1") -Artifacts $retentionArtifacts

# -- Cleanup auto-provisioned infrastructure (reverse order) --

if ($script:startedDocker) {
    Write-Host "Stopping Docker NAT containers..." -ForegroundColor Yellow
    Stop-DockerNat
}

# Overnight unattended runs should not leave emulator processes burning CPU/GPU
Stop-TestSuiteEmulators

# Note: tests that own a server lifecycle clean it up themselves

if ($failCount -gt 0 -or $timeoutCount -gt 0 -or $notRun.Count -gt 0) { exit 1 } else { exit 0 }
