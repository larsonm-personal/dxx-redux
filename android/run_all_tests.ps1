#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Unattended sequential test runner with pass/fail report.

.DESCRIPTION
    Discovers all json5 and ps1 tests, determines which are runnable based on
    current environment (emulator running, two emulators, server, game data),
    runs them sequentially, and produces a summary report.

    Designed to run overnight: continue-on-failure, stdout/stderr capture per
    test, wall-clock timing, final markdown + console summary.

.PARAMETER Filter
    Glob filter for test names (e.g. "test_death*").

.PARAMETER IncludeManual
    Include tests that are normally skipped (dual-emu setup, manual tests).

.PARAMETER StopOnFail
    Stop after the first failure instead of continuing.

.PARAMETER ReportDir
    Directory for per-test output logs (default: temp\test_reports).

.EXAMPLE
    .\run_all_tests.ps1
    .\run_all_tests.ps1 -Filter "test_death*"
    .\run_all_tests.ps1 -StopOnFail
#>

param(
    [string]$Filter,
    [switch]$IncludeManual,
    [switch]$StopOnFail,
    [string]$ReportDir
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

# -- Discover environment --

function Test-SingleEmulator {
    $out = Adb-Timeout -AdbArgs @("devices") -Seconds 5
    if ($out -match "emulator-\d+\s+device") { return $true }
    return $false
}

function Test-TwoEmulators {
    $out = Adb-Timeout -AdbArgs @("devices") -Seconds 5
    if (-not $out) { return $false }
    $matches = [regex]::Matches($out, "emulator-\d+\s+device")
    return ($matches.Count -ge 2)
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

$hasEmu = Test-SingleEmulator
$hasTwoEmu = Test-TwoEmulators
$hasServer = Test-MatchmakingServer
$hasGameData = Test-GameDataAvailable

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  DXX-Redux Unattended Test Suite" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Environment:" -ForegroundColor White
Write-Host "  Single emulator:  $(if ($hasEmu) { 'YES' } else { 'NO' })"
Write-Host "  Two emulators:    $(if ($hasTwoEmu) { 'YES' } else { 'NO' })"
Write-Host "  Match server:     $(if ($hasServer) { 'YES' } else { 'NO' })"
Write-Host "  Game data (CDs):  $(if ($hasGameData) { 'YES' } else { 'NO' })"
Write-Host ""

# -- Build test catalog --

# Tests that are always skipped in unattended mode
$manualTests = @(
    "test_keyboard_manual"     # requires human interaction
    "test_dual_emu"            # interactive menu
    "test_dual_emu_setup"      # interactive setup
)

# Tests requiring two emulators
$twoEmuTests = @(
    "test_mp"
    "test_lan"
    "test_lan_discovery"
)

# Tests requiring matchmaking server
$serverTests = @(
    "test_bot_client"
)

# Tests requiring game data / CD images
$gameDataTests = @(
    "test_extract"
    "test_all_extracts"
)

$allTests = @()

# json5 game-automation scripts (run via run_test.ps1)
$gameScriptsDir = Join-Path $scriptDir "game_scripts"
$json5Files = @(Get-ChildItem -Path $gameScriptsDir -Filter "test_*.json5" -File -ErrorAction SilentlyContinue | Sort-Object Name)
foreach ($t in $json5Files) {
    $allTests += @{
        Name = $t.BaseName
        Type = "json5"
        Path = $t.FullName
        Requires = "emulator"
    }
}

# ps1 integration tests
$testsDir = Join-Path $scriptDir "tests"
$ps1Files = @(Get-ChildItem -Path $testsDir -Filter "test_*.ps1" -File -ErrorAction SilentlyContinue | Sort-Object Name)
foreach ($t in $ps1Files) {
    $name = $t.BaseName
    $req = "none"
    if ($name -in $twoEmuTests) { $req = "two_emulators" }
    elseif ($name -in $serverTests) { $req = "server" }
    elseif ($name -in $gameDataTests) { $req = "game_data" }
    elseif ($name -in @("test_cue_iso", "test_server_integration")) { $req = "none" }
    else { $req = "emulator" }

    $allTests += @{
        Name = $name
        Type = "ps1"
        Path = $t.FullName
        Requires = $req
    }
}

# Apply filter
if ($Filter) {
    $allTests = @($allTests | Where-Object { $_.Name -like $Filter })
}

# Classify each test
$results = @()
$runnable = @()
$skipped = @()

foreach ($test in $allTests) {
    $name = $test.Name
    $skip = $null

    if ((-not $IncludeManual) -and ($name -in $manualTests)) {
        $skip = "manual/interactive"
    }
    elseif ($test.Requires -eq "emulator" -and -not $hasEmu) {
        $skip = "no emulator"
    }
    elseif ($test.Requires -eq "two_emulators" -and -not $hasTwoEmu) {
        $skip = "need 2 emulators"
    }
    elseif ($test.Requires -eq "server" -and -not $hasServer) {
        $skip = "no matchmaking server"
    }
    elseif ($test.Requires -eq "game_data" -and -not $hasGameData) {
        $skip = "no game data"
    }

    if ($skip) {
        $skipped += @{ Name = $name; Reason = $skip; Type = $test.Type }
    } else {
        $runnable += $test
    }
}

Write-Host "Tests found: $($allTests.Count) total, $($runnable.Count) runnable, $($skipped.Count) skipped" -ForegroundColor White
Write-Host ""

if ($runnable.Count -eq 0) {
    Write-Host "No runnable tests found." -ForegroundColor Yellow
    exit 0
}

# -- Run tests sequentially --

$runTestScript = Join-Path $scriptDir "run_test.ps1"
$totalSw = [System.Diagnostics.Stopwatch]::StartNew()
$passCount = 0
$failCount = 0
$stopEarly = $false

foreach ($test in $runnable) {
    if ($stopEarly) { break }

    $name = $test.Name
    $logFile = Join-Path $ReportDir "${name}_${timestamp}.log"

    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  Running: $name  [$($test.Type)]" -ForegroundColor White
    $sw = [System.Diagnostics.Stopwatch]::StartNew()

    try {
        if ($test.Type -eq "json5") {
            $scriptFile = [System.IO.Path]::GetFileName($test.Path)
            & $runTestScript $scriptFile *>&1 | Tee-Object -FilePath $logFile
            $exitCode = $LASTEXITCODE
        } else {
            & $test.Path *>&1 | Tee-Object -FilePath $logFile
            $exitCode = $LASTEXITCODE
        }
    } catch {
        $exitCode = 1
        "EXCEPTION: $_" | Out-File -FilePath $logFile -Append -Encoding utf8
        Write-Host "  EXCEPTION: $_" -ForegroundColor Red
    }

    $sw.Stop()
    $elapsed = $sw.Elapsed.ToString("mm\:ss")

    $status = if ($exitCode -eq 0) { "PASS" } else { "FAIL" }
    $color = if ($exitCode -eq 0) { "Green" } else { "Red" }
    Write-Host "  $status ($elapsed)" -ForegroundColor $color

    if ($exitCode -eq 0) { $passCount++ } else { $failCount++ }

    $results += @{
        Name = $name
        Type = $test.Type
        Status = $status
        ExitCode = $exitCode
        Elapsed = $elapsed
        LogFile = $logFile
    }

    if ($StopOnFail -and $exitCode -ne 0) {
        Write-Host "  Stopping early (-StopOnFail)" -ForegroundColor Yellow
        $stopEarly = $true
    }
}

$totalSw.Stop()
$totalElapsed = $totalSw.Elapsed.ToString("hh\:mm\:ss")

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
    Write-Host "  [$icon] $($r.Status.PadRight(4))  $($r.Elapsed)  $($r.Name)" -ForegroundColor $color
}
foreach ($s in $skipped) {
    Write-Host "  [-] SKIP        $($s.Name)  ($($s.Reason))" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "  Passed: $passCount  Failed: $failCount  Skipped: $($skipped.Count)  Total time: $totalElapsed"
Write-Host ""

# Markdown report
$md = @()
$md += "# Test Report - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$md += ""
$md += "## Environment"
$md += "- Single emulator: $(if ($hasEmu) { 'yes' } else { 'no' })"
$md += "- Two emulators: $(if ($hasTwoEmu) { 'yes' } else { 'no' })"
$md += "- Matchmaking server: $(if ($hasServer) { 'yes' } else { 'no' })"
$md += "- Game data (CDs): $(if ($hasGameData) { 'yes' } else { 'no' })"
$md += ""
$md += "## Summary"
$md += "- Passed: $passCount"
$md += "- Failed: $failCount"
$md += "- Skipped: $($skipped.Count)"
$md += "- Total time: $totalElapsed"
$md += ""
$md += "## Results"
$md += ""
$md += "| Status | Time | Test | Type |"
$md += "|--------|------|------|------|"
foreach ($r in $results) {
    $md += "| $($r.Status) | $($r.Elapsed) | $($r.Name) | $($r.Type) |"
}
foreach ($s in $skipped) {
    $md += "| SKIP | -- | $($s.Name) | $($s.Type) ($($s.Reason)) |"
}
$md += ""

if ($failCount -gt 0) {
    $md += "## Failures"
    $md += ""
    foreach ($r in ($results | Where-Object { $_.Status -eq "FAIL" })) {
        $md += "### $($r.Name)"
        $md += "- Exit code: $($r.ExitCode)"
        $md += "- Log: ``$(Split-Path $r.LogFile -Leaf)``"
        # Include last 20 lines of the log
        if (Test-Path $r.LogFile) {
            $tail = Get-Content $r.LogFile -Tail 20
            $md += '```'
            $md += $tail
            $md += '```'
        }
        $md += ""
    }
}

$md -join "`n" | Set-Content -Path $reportFile -Encoding utf8
Write-Host "  Report: $reportFile" -ForegroundColor Cyan
Write-Host ""

if ($failCount -gt 0) { exit 1 } else { exit 0 }
