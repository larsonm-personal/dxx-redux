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

. "$PSScriptRoot\test_helpers.ps1"

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

function Read-Json5 {
    param([string]$Path)
    $raw = Get-Content $Path -Raw
    $raw = [regex]::Replace($raw, '//.*', '')
    $raw = [regex]::Replace($raw, ',\s*([}\]])', '$1')
    return ($raw | ConvertFrom-Json)
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

    # Read prior test result from spec file (if any)
    $priorStatus = $null
    try {
        $specData = Read-Json5 $specPath
        if ($specData.last_test_result) { $priorStatus = $specData.last_test_result.status }
    } catch {}

    # Pre-flight: verify emulator is healthy before each test
    if (-not (Test-EmulatorHealthy)) {
        try { Ensure-EmulatorHealthy } catch {
            Write-Host "  Emulator not recoverable. Stopping." -ForegroundColor Red
            $results += [PSCustomObject]@{ Source = $sourceName; Status = 'FAIL'; Time = '00:00'; ExitCode = 98 }
            $failures++
            break
        }
    }

    # Force-stop app between tests to ensure clean state
    Adb -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
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
    Adb -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
    # Child script may change ErrorActionPreference; reset it
    $ErrorActionPreference = 'Stop'

    $elapsed = (Get-Date) - $testStart
    $status = if ($exitCode -eq 0) { 'PASS' } else { 'FAIL' }

    # Read new result from spec file (written by Exit-Test)
    $newStatus = $null
    try {
        $specData = Read-Json5 $specPath
        if ($specData.last_test_result) { $newStatus = $specData.last_test_result.status }
    } catch {}
    $changed = ($priorStatus -ne $newStatus)

    $results += [PSCustomObject]@{
        Source   = $sourceName
        Status   = $status
        Time     = '{0:mm\:ss}' -f $elapsed
        ExitCode = $exitCode
        Prior    = if ($priorStatus) { $priorStatus } else { '-' }
        Saved    = if ($newStatus) { $newStatus } else { '-' }
        Changed  = if ($changed -and $priorStatus) { '*' } else { '' }
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
$changedCount = @($results | Where-Object { $_.Changed -eq '*' }).Count

Write-Host ""
Write-Host "============================================================" -ForegroundColor White
Write-Host "  RESULTS: $passed/$($results.Count) passed, $failures failed" -ForegroundColor $(if ($failures -eq 0) { 'Green' } else { 'Red' })
if ($changedCount -gt 0) {
    Write-Host "  Spec results changed: $changedCount" -ForegroundColor Yellow
}
Write-Host "  Total time: $( '{0:hh\:mm\:ss}' -f $totalElapsed )" -ForegroundColor White
Write-Host "============================================================" -ForegroundColor White
Write-Host ""

$results | Format-Table Source, Status, Prior, Saved, Changed, Time, ExitCode -AutoSize

# Final cleanup: ensure app is stopped
Adb-Quick -CmdArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null

# Exit code = number of failures
exit $failures
