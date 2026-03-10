#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Run extraction regression tests across all or selected sources.

.DESCRIPTION
  Finds extract_regression.json5 specs under game_data/ and runs each through
  run_extract_test.ps1. Produces a summary table of results.

.PARAMETER Filter
  Glob/wildcard filter for source directory names (e.g. "Descent II*").

.PARAMETER SkipLaunch
  Pass -SkipLaunch to each test (file-only verification).

.PARAMETER MaxFailures
  Stop after this many failures (default: unlimited).

.PARAMETER SpecPaths
  Explicit list of spec file paths to run (overrides auto-discovery).

.EXAMPLE
  .\run_all_extract_tests.ps1                          # all specs
  .\run_all_extract_tests.ps1 -Filter "Descent II*"   # D2 CDs only
  .\run_all_extract_tests.ps1 -SkipLaunch              # file-only
#>
param(
    [string]$Filter,
    [switch]$SkipLaunch,
    [int]$MaxFailures = 0,
    [string[]]$SpecPaths
)

$ErrorActionPreference = 'Stop'
$SCRIPT_DIR = $PSScriptRoot
$REPO_ROOT = Split-Path $SCRIPT_DIR -Parent
$GAME_DATA = Join-Path $REPO_ROOT 'game_data'
$TEST_SCRIPT = Join-Path $SCRIPT_DIR 'run_extract_test.ps1'

# ── Discover specs ───────────────────────────────────────────

if ($SpecPaths -and $SpecPaths.Count -gt 0) {
    $specs = @()
    foreach ($sp in $SpecPaths) {
        $resolved = Resolve-Path $sp -ErrorAction SilentlyContinue
        if ($resolved) { $specs += $resolved.Path }
        else { Write-Warning "Spec not found: $sp" }
    }
} else {
    $specs = Get-ChildItem $GAME_DATA -Recurse -Filter '*_regression.json5' |
        Sort-Object FullName |
        ForEach-Object { $_.FullName }

    if ($Filter) {
        $specs = $specs | Where-Object {
            $parent = Split-Path (Split-Path $_ -Parent) -Leaf
            $parent -like $Filter
        }
    }
}

if ($specs.Count -eq 0) {
    Write-Host "No regression specs found." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor White
Write-Host "  EXTRACTION REGRESSION TEST SUITE" -ForegroundColor White
Write-Host "  Specs: $($specs.Count)" -ForegroundColor White
Write-Host "  Mode:  $(if ($SkipLaunch) { 'file-only (-SkipLaunch)' } else { 'full (with launch)' })" -ForegroundColor White
Write-Host "============================================================" -ForegroundColor White
Write-Host ""

# ── Helpers ──────────────────────────────────────────────────

$_depBaseFile = Join-Path $REPO_ROOT 'dependency_base.txt'
$DEP_BASE = (Get-Content $_depBaseFile -First 1).Trim()
$ADB = "$DEP_BASE\android-sdk\platform-tools\adb.exe"
$HEALTH_SCRIPT = Join-Path $SCRIPT_DIR 'emu_health.ps1'
$PACKAGE = 'com.dxxredux.app'

function Adb-Quick {
    # Quick adb call with timeout, used only for health checks in the orchestrator.
    param([string[]]$CmdArgs, [int]$Timeout = 10)
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $ADB
    $psi.Arguments = ($CmdArgs | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join ' '
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $proc = [System.Diagnostics.Process]::Start($psi)
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    $stdout = $proc.StandardOutput.ReadToEnd()
    if (-not $proc.WaitForExit($Timeout * 1000)) {
        try { $proc.Kill() } catch {}
        $proc.Dispose()
        return ''
    }
    $proc.Dispose()
    return $stdout.Trim()
}

function Test-EmulatorReady {
    # Check emulator + activity manager are both responsive
    $devices = Adb-Quick -CmdArgs 'devices'
    if ($devices -notmatch 'emulator-\d+\s+device') { return $false }
    $amResult = Adb-Quick -CmdArgs @('shell', 'am', 'get-config')
    return ($amResult -and $amResult -notmatch "Can't find service" -and $amResult -match '\w')
}

function Repair-Emulator {
    # Try to recover the emulator. Returns $true if successful.
    Write-Host "  Emulator/AM unhealthy. Attempting recovery..." -ForegroundColor Yellow

    # First try: just force-stop the app and wait for AM to recover
    Adb-Quick -CmdArgs @('shell', 'am', 'force-stop', 'com.dxxredux.app') | Out-Null
    Start-Sleep -Seconds 5
    if (Test-EmulatorReady) {
        Write-Host "  Recovered after force-stop" -ForegroundColor Green
        return $true
    }

    # Second try: full emulator restart via emu_health.ps1
    if (Test-Path $HEALTH_SCRIPT) {
        Write-Host "  Restarting emulator..." -ForegroundColor Yellow
        & $HEALTH_SCRIPT -Restart -Wait -TimeoutSeconds 180
        Start-Sleep -Seconds 5
        if (Test-EmulatorReady) {
            Write-Host "  Recovered after emulator restart" -ForegroundColor Green
            return $true
        }
    }

    return $false
}

# ── Run tests ────────────────────────────────────────────────

$results = @()
$failures = 0
$startTime = Get-Date

foreach ($specPath in $specs) {
    $specDir = Split-Path $specPath -Parent
    $sourceName = Split-Path $specDir -Leaf
    # For GOG specs, use the spec filename as the source name
    if ($specPath -match '_regression\.json5$' -and $specPath -notmatch 'extract_regression\.json5$') {
        $sourceName = [System.IO.Path]::GetFileNameWithoutExtension($specPath) -replace '_regression$', ''
    }

    Write-Host "─── [$($results.Count + 1)/$($specs.Count)] $sourceName ───" -ForegroundColor Cyan

    # Pre-flight: verify emulator is healthy before each test
    if (-not (Test-EmulatorReady)) {
        if (-not (Repair-Emulator)) {
            Write-Host "  Emulator not recoverable. Stopping." -ForegroundColor Red
            $results += [PSCustomObject]@{ Source = $sourceName; Status = 'FAIL'; Time = '00:00'; ExitCode = 98 }
            $failures++
            break
        }
    }

    # Force-stop app between tests to ensure clean state
    Adb-Quick -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
    Start-Sleep -Seconds 1

    $testStart = Get-Date
    $testParams = @{ SpecPath = $specPath }
    if ($SkipLaunch) { $testParams['SkipLaunch'] = $true }

    $exitCode = 0
    try {
        & $TEST_SCRIPT @testParams
        $exitCode = $LASTEXITCODE
    } catch {
        Write-Host "  EXCEPTION: $_" -ForegroundColor Red
        $exitCode = 99
    }

    # Force-stop after each test regardless of outcome
    Adb-Quick -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
    # Child script may change ErrorActionPreference; reset it
    $ErrorActionPreference = 'Stop'

    $elapsed = (Get-Date) - $testStart
    $status = if ($exitCode -eq 0) { 'PASS' } else { 'FAIL' }

    $results += [PSCustomObject]@{
        Source   = $sourceName
        Status   = $status
        Time     = '{0:mm\:ss}' -f $elapsed
        ExitCode = $exitCode
    }

    if ($status -eq 'FAIL') {
        $failures++
        if ($MaxFailures -gt 0 -and $failures -ge $MaxFailures) {
            Write-Host "Stopping after $failures failure(s) (-MaxFailures $MaxFailures)" -ForegroundColor Red
            break
        }
    }

    Write-Host ""
}

# ── Summary ──────────────────────────────────────────────────

$totalElapsed = (Get-Date) - $startTime
$passed = @($results | Where-Object { $_.Status -eq 'PASS' }).Count

Write-Host ""
Write-Host "============================================================" -ForegroundColor White
Write-Host "  RESULTS: $passed/$($results.Count) passed, $failures failed" -ForegroundColor $(if ($failures -eq 0) { 'Green' } else { 'Red' })
Write-Host "  Total time: $( '{0:hh\:mm\:ss}' -f $totalElapsed )" -ForegroundColor White
Write-Host "============================================================" -ForegroundColor White
Write-Host ""

$results | Format-Table Source, Status, Time, ExitCode -AutoSize

# Final cleanup: ensure app is stopped
Adb-Quick -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null

# Exit code = number of failures
exit $failures
