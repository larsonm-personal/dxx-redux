#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Unattended sequential test runner with automatic infrastructure setup.

.DESCRIPTION
    Discovers all json5 and ps1 tests, automatically provisions required
    infrastructure (emulators, matchmaking server, Docker NAT containers),
    runs tests sequentially, and produces a summary report.

    Infrastructure is brought up on demand and torn down at the end:
      1. No-infra tests (game_data extraction, cue/iso, server unit tests)
      2. Single emulator started, APK installed, game data pushed
      3. Single-emulator tests (json5 automation + ps1 emulator tests)
      4. Second emulator started, APK installed, game data pushed
      5. Matchmaking server started
      6. Two-emulator + server tests (multiplayer, LAN, bot client)
      7. Docker NAT tests (if Docker is available)
      8. Cleanup in reverse order

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
        $ver = docker version --format '{{.Server.Version}}' 2>&1
        return ($LASTEXITCODE -eq 0)
    } catch { return $false }
}

# -- Test catalog --

# Tests always skipped in unattended mode
$manualTests = @(
    "test_keyboard_manual"     # requires human interaction
    "test_dual_emu"            # interactive menu
    "test_dual_emu_setup"      # interactive setup
)

# Infrastructure requirement classification
$twoEmuTests = @("test_mp", "test_lan", "test_lan_discovery", "test_lan_broadcast", "test_lan_lobby_discovery")
$serverTests = @("test_bot_client")

# Per-test timeout overrides (seconds) for multi-phase tests
$testTimeouts = @{
    "test_autoselect_crash_unified"       = 240
    "test_gog_installer_redbook_unified"  = 300
    "test_saf_archiver"                   = 360
    "test_all_extracts"                   = 300
    "test_mp"                             = 240
}
$extractTests = @("test_extract", "test_all_extracts")  # emulator + game data, run last
$noInfraTests = @("test_cue_iso", "test_server_integration")

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
    $req = "none"
    if ($name -in $twoEmuTests) { $req = "two_emulators" }
    elseif ($name -in $serverTests) { $req = "server" }
    elseif ($name -in $extractTests) { $req = "extract" }
    elseif ($name -in $noInfraTests) { $req = "none" }
    else { $req = "emulator" }

    $allTests += @{
        Name = $name
        Type = "ps1"
        Path = $t.FullName
        Requires = $req
        TimeoutSeconds = if ($testTimeouts.ContainsKey($name)) { $testTimeouts[$name] } else { 0 }
    }
}

# Apply filter
if ($Filter) {
    $allTests = @($allTests | Where-Object { $_.Name -like $Filter })
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

# Group by infrastructure tier
$tierNone = @($runnableTests | Where-Object { $_.Requires -eq "none" })
$tierSingleEmu = @($runnableTests | Where-Object { $_.Requires -eq "emulator" })
$tierDualEmu = @($runnableTests | Where-Object { $_.Requires -eq "two_emulators" -or $_.Requires -eq "server" })
$tierExtract = @($runnableTests | Where-Object { $_.Requires -eq "extract" })

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  DXX-Redux Unattended Test Suite" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tests found: $($allTests.Count) total, $($runnableTests.Count) runnable, $($manualSkipped.Count) manual-skipped" -ForegroundColor White
Write-Host "  Tier 0 (no infra):       $($tierNone.Count)"
Write-Host "  Tier 1 (single emu):     $($tierSingleEmu.Count)"
Write-Host "  Tier 2 (dual emu/server): $($tierDualEmu.Count)"
Write-Host "  Tier 3 (extract/slow):   $($tierExtract.Count)"
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

function Run-SingleTest {
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
        $scriptFile = [System.IO.Path]::GetFileName($Test.Path)
        $testCmd = "& '$runTestScript' '$scriptFile'"
    } elseif ($Test.Name -eq "test_saf_archiver") {
        $testCmd = "& '$($Test.Path)' -NoBuild"
    } else {
        $testCmd = "& '$($Test.Path)'"
    }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "pwsh"
    $psi.Arguments = "-NoProfile -NonInteractive -Command `"$testCmd; exit `$LASTEXITCODE`""
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

    $script:results += @{
        Name = $name; Type = $Test.Type; Status = $status
        ExitCode = $exitCode; Elapsed = $elapsed; LogFile = $logFile
    }

    if ($StopOnFail -and $exitCode -ne 0) {
        Write-Host "  Stopping early (-StopOnFail)" -ForegroundColor Yellow
        $script:stopEarly = $true
    }
}

# ── Tier 0: no-infrastructure tests ─────────────────────────────────────

if ($tierNone.Count -gt 0 -and -not $stopEarly) {
    Write-Host ""
    Write-Host "== Tier 0: No-infrastructure tests ==" -ForegroundColor Cyan
    foreach ($test in $tierNone) {
        if ($stopEarly) { break }
        Run-SingleTest -Test $test
    }
}

# ── Tier 1: single-emulator tests ───────────────────────────────────────

if ($tierSingleEmu.Count -gt 0 -and -not $stopEarly) {
    Write-Host ""
    Write-Host "== Tier 1: Single-emulator tests ==" -ForegroundColor Cyan

    # Kill stale ADB server to prevent hangs on first device command.
    # A zombie adb.exe from a prior session can block indefinitely.
    Write-Host "  Restarting ADB server..." -ForegroundColor DarkGray
    Get-Process adb -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    & $script:ADB start-server 2>$null
    Start-Sleep -Seconds 2

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

        foreach ($test in $tierSingleEmu) {
            if ($stopEarly) { break }
            Run-SingleTest -Test $test
        }
    } else {
        foreach ($test in $tierSingleEmu) {
            $infraSkipped += @{ Name = $test.Name; Reason = "could not start emulator"; Type = $test.Type }
        }
    }
}

# ── Tier 2: dual-emulator + server tests ────────────────────────────────

if ($tierDualEmu.Count -gt 0 -and -not $stopEarly) {
    Write-Host ""
    Write-Host "== Tier 2: Dual-emulator + server tests ==" -ForegroundColor Cyan

    # Ensure first emulator is running (may already be from tier 1)
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
        if ($serials.Count -ge 2) {
            $emu2Serial = $serials | Select-Object -Last 1
            Install-AppAndData -Serial $emu2Serial
        }

        foreach ($test in $tierDualEmu) {
            if ($stopEarly) { break }
            Run-SingleTest -Test $test
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

# ── Tier 3: extract regression tests (slow, run last) ───────────────────

if ($tierExtract.Count -gt 0 -and -not $stopEarly) {
    Write-Host ""
    Write-Host "== Tier 3: Extract regression tests (slow) ==" -ForegroundColor Cyan

    $hasGameData = Test-GameDataAvailable
    if (-not $hasGameData) {
        foreach ($test in $tierExtract) {
            $infraSkipped += @{ Name = $test.Name; Reason = "no game data (CD images)"; Type = $test.Type }
        }
    } else {
        # Ensure emulator is running (may already be from tier 1/2)
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
                Run-SingleTest -Test $test
            }
        } else {
            foreach ($test in $tierExtract) {
                $infraSkipped += @{ Name = $test.Name; Reason = "could not start emulator"; Type = $test.Type }
            }
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

if ($failCount -gt 0) {
    $md += "## Failures"
    $md += ""
    foreach ($r in ($results | Where-Object { $_.Status -eq "FAIL" })) {
        $md += "### $($r.Name)"
        $md += "- Exit code: $($r.ExitCode)"
        $md += "- Log: ``$(Split-Path $r.LogFile -Leaf)``"
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

# -- Cleanup auto-provisioned infrastructure (reverse order) --

if ($script:startedDocker) {
    Write-Host "Stopping Docker NAT containers..." -ForegroundColor Yellow
    Stop-DockerNat
}

if ($script:autoServerProc -and -not $script:autoServerProc.HasExited) {
    Write-Host "Stopping matchmaking server (PID $($script:autoServerProc.Id))..." -ForegroundColor Yellow
    try { $script:autoServerProc.Kill(); $script:autoServerProc.WaitForExit(5000) } catch {}
}

# Note: we do NOT stop emulators we started -- they're useful for the next run
# and stopping them is slow. The user can stop them manually if desired.

if ($failCount -gt 0) { exit 1 } else { exit 0 }
