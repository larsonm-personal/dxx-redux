#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Run extraction regression tests across all or selected sources.

.DESCRIPTION
  Finds extract_regression.json5 specs under game_data/ and runs each through
  test_extract.ps1. Produces a summary table of results.

.PARAMETER Filter
  Glob/wildcard filter for source directory names (e.g. "Descent II*").

.PARAMETER SkipLaunch
  Pass -SkipLaunch to each test (file-only verification).

.PARAMETER MaxFailures
  Stop after this many failures (default: unlimited).

.PARAMETER All
  Run all specs instead of picking one at random.

.PARAMETER SampleCount
  Run this many randomly ordered specs. Defaults to 1 unless -All, -SpecPaths, or -Filter is given.

.PARAMETER Seed
  Integer seed for repeatable random selection.

.PARAMETER RandomOrder
  Shuffle the selected specs before running them. Use with -All to run every spec in seeded order.

.PARAMETER SpecPaths
  Explicit list of spec file paths to run (overrides auto-discovery).

.PARAMETER BuildAndInstall
  Build the current debug APK and reinstall it before device preflight.

.PARAMETER RestartDevice
  Force a clean emulator restart before device preflight.

.EXAMPLE
  .\test_all_extracts.ps1                          # one random spec
  .\test_all_extracts.ps1 -All                     # all specs
    .\test_all_extracts.ps1 -SampleCount 2 -Seed 123 # repeatable sample
  .\test_all_extracts.ps1 -Filter "Descent II*"    # D2 CDs only
  .\test_all_extracts.ps1 -SkipLaunch              # file-only
#>
param(
    [string]$Filter,
    [switch]$SkipLaunch,
    [switch]$All,
    [int]$SampleCount = 0,
    [int]$Seed = 0,
    [switch]$RandomOrder,
    [int]$MaxFailures = 0,
    [string[]]$SpecPaths,
    [switch]$BuildAndInstall,
    [switch]$RestartDevice
)

$ErrorActionPreference = 'Stop'
$SCRIPT_DIR = $PSScriptRoot
$REPO_ROOT = Split-Path (Split-Path $SCRIPT_DIR)
$GAME_DATA = Join-Path $REPO_ROOT 'game_data'
$TEST_SCRIPT = Join-Path $SCRIPT_DIR 'test_extract.ps1'

Write-Host "test_all_extracts.ps1 starting"

. "$PSScriptRoot\..\helpers\test_helpers.ps1"
. (Join-Path $PSScriptRoot 'extract_regression_spec_helpers.ps1')

if (-not $env:ANDROID_SERIAL) {
    $env:ANDROID_SERIAL = $script:PRIMARY_EMULATOR_SERIAL
}

if (-not (Ensure-ExtractRegressionOracles -RepoRoot $REPO_ROOT -Context "test_all_extracts.ps1")) {
    exit 1
}

# -- Discover specs -------------------------------------------

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
    Write-Host "No regression specs found" -ForegroundColor Red
    exit 1
}

if ($SampleCount -lt 0) {
    Write-Host "FAIL: -SampleCount must be zero or greater" -ForegroundColor Red
    exit 1
}
if ($All -and $SampleCount -gt 0) {
    Write-Host "FAIL: -All and -SampleCount cannot be used together" -ForegroundColor Red
    exit 1
}

function Get-ShuffledSpecs {
    param(
        [string[]]$Items,
        [int]$ShuffleSeed,
        [bool]$HasSeed
    )

    $shuffled = @($Items)
    $random = if ($HasSeed) { [System.Random]::new($ShuffleSeed) } else { [System.Random]::new() }
    for ($i = $shuffled.Count - 1; $i -gt 0; $i--) {
        $j = $random.Next($i + 1)
        $tmp = $shuffled[$i]
        $shuffled[$i] = $shuffled[$j]
        $shuffled[$j] = $tmp
    }
    return , $shuffled
}

$seedSpecified = $PSBoundParameters.ContainsKey('Seed')
$effectiveSampleCount = 0
if (-not $All) {
    if ($SampleCount -gt 0) {
        $effectiveSampleCount = $SampleCount
    } elseif (-not $SpecPaths -and -not $Filter -and $specs.Count -gt 1) {
        $effectiveSampleCount = 1
    }
}

$availableSpecCount = $specs.Count
if ($effectiveSampleCount -gt 0 -or $RandomOrder) {
    $specs = Get-ShuffledSpecs -Items $specs -ShuffleSeed $Seed -HasSeed $seedSpecified
}
if ($effectiveSampleCount -gt 0 -and $specs.Count -gt $effectiveSampleCount) {
    $specs = @($specs | Select-Object -First $effectiveSampleCount)
}
if ($effectiveSampleCount -gt 0) {
    $seedText = if ($seedSpecified) { ", seed $Seed" } else { "" }
    Write-Host "Selected extraction sample: $($specs.Count)/$availableSpecCount spec(s)$seedText" -ForegroundColor Yellow
    Write-Host "  (use -All to run all $availableSpecCount specs)" -ForegroundColor DarkGray
} elseif ($RandomOrder) {
    $seedText = if ($seedSpecified) { " with seed $Seed" } else { "" }
    Write-Host "Randomized extraction order for $availableSpecCount spec(s)$seedText" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor White
Write-Host "  EXTRACTION REGRESSION TEST SUITE" -ForegroundColor White
Write-Host "  Specs: $($specs.Count)" -ForegroundColor White
Write-Host "  Mode:  $(if ($SkipLaunch) { 'file-only (-SkipLaunch)' } else { 'full (with launch)' })" -ForegroundColor White
Write-Host "============================================================" -ForegroundColor White
Write-Host ""

# -- Helpers --------------------------------------------------

function Read-Json5 {
    param([string]$Path)
    return Read-Json5File $Path
}

if ($BuildAndInstall) {
    $gradle = Join-Path (Join-Path $REPO_ROOT 'android') 'gradlew.bat'
    Write-Host 'Building current debug APK for extraction regressions...' -ForegroundColor Cyan
    & $gradle -p (Join-Path $REPO_ROOT 'android') assembleDebug
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL: Debug APK build failed with exit code $LASTEXITCODE" -ForegroundColor Red
        exit 97
    }

}

if ($RestartDevice) {
    if (-not (Invoke-LauncherStartupRecovery -Reason 'Preparing clean extraction regression device')) {
        Write-Host 'FAIL: Extraction test device restart failed' -ForegroundColor Red
        exit 98
    }
}

if ($BuildAndInstall) {
    Ensure-EmulatorHealthy | Out-Null
    Write-Host 'Installing current debug APK for extraction regressions...' -ForegroundColor Cyan
    if (-not (Install-ApkOnDevice)) {
        Write-Host 'FAIL: Debug APK install failed' -ForegroundColor Red
        exit 97
    }
}

Write-Host "Preflighting emulator, APK, and SetupActivity..." -ForegroundColor Cyan
if (-not (Ensure-LauncherTestDeviceReady)) {
    Write-Host "FAIL: Extraction test device could not start SetupActivity" -ForegroundColor Red
    exit 98
}
Write-Host "Extraction test device ready" -ForegroundColor Green

# -- Run tests ------------------------------------------------

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

    Write-Host "--- [$($results.Count + 1)/$($specs.Count)] $sourceName ---" -ForegroundColor Cyan

    # Read prior test result from spec file (if any)
    $priorStatus = $null
    try {
        $specData = Read-Json5 $specPath
        if ($specData.last_test_result) { $priorStatus = $specData.last_test_result.status }
    } catch {}

    # Pre-flight: verify emulator is healthy before each test
    if (-not (Test-EmulatorHealthy)) {
        try {
            if (-not (Ensure-LauncherTestDeviceReady)) {
                throw 'SetupActivity recovery failed'
            }
        } catch {
            Write-Host "  Emulator not recoverable. Stopping" -ForegroundColor Red
            $results += [PSCustomObject]@{ Source = $sourceName; Status = 'FAIL'; Time = '00:00'; ExitCode = 98; Attempts = 0 }
            $failures++
            break
        }
    }

    # Force-stop app between tests to ensure clean state
    Adb -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
    Start-Sleep -Seconds 1

    $testStart = Get-Date
    # Run test_extract as a child process so its `exit` call doesn't kill us
    $testArgs = @("-NoProfile", "-NonInteractive", "-File", $TEST_SCRIPT, "-SpecPath", $specPath)
    if ($SkipLaunch) { $testArgs += "-SkipLaunch" }

    $exitCode = 0
    $attempt = 0
    while ($true) {
        $attempt++
        try {
            pwsh @testArgs
            $exitCode = $LASTEXITCODE
        } catch {
            Write-Host "  EXCEPTION: $_" -ForegroundColor Red
            $exitCode = 99
        }
        if ($exitCode -ne 98 -or $attempt -gt 1) {
            break
        }

        Write-Host "  Extraction infrastructure failed. Recovering ADB/device state and rerunning the complete spec once" -ForegroundColor Yellow
        if (-not (Confirm-EmulatorHealthWithAdbRecovery)) {
            if (-not (Invoke-LauncherStartupRecovery -Reason 'Extraction infrastructure failure')) {
                Write-Host "  Emulator restart failed" -ForegroundColor Red
                break
            }
        }
        if (-not (Ensure-LauncherTestDeviceReady)) {
            Write-Host "  Launcher recovery failed" -ForegroundColor Red
            break
        }
    }

    # Force-stop after each test regardless of outcome
    Adb -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null
    # Child script may change ErrorActionPreference; reset it
    $ErrorActionPreference = 'Stop'

    $elapsed = (Get-Date) - $testStart

    # Read new result from spec file (written by Exit-Test)
    $newStatus = $null
    try {
        $specData = Read-Json5 $specPath
        if ($specData.last_test_result) { $newStatus = $specData.last_test_result.status }
    } catch {}
    $status = if ($exitCode -ne 0) {
        'FAIL'
    } elseif ($newStatus -eq 'skip') {
        'SKIP'
    } else {
        'PASS'
    }
    $changed = ($priorStatus -ne $newStatus)

    $results += [PSCustomObject]@{
        Source   = $sourceName
        Status   = $status
        Time     = '{0:mm\:ss}' -f $elapsed
        ExitCode = $exitCode
        Prior    = if ($priorStatus) { $priorStatus } else { '-' }
        Saved    = if ($newStatus) { $newStatus } else { '-' }
        Changed  = if ($changed -and $priorStatus) { '*' } else { '' }
        Attempts = $attempt
    }

    if ($status -eq 'FAIL') {
        $failures++
        if ($exitCode -in @(98, 99)) {
            $failureKind = if ($exitCode -eq 98) { 'infrastructure failure' } else { 'runner error' }
            Write-Host "Stopping after extraction test $failureKind" -ForegroundColor Red
            break
        }
        if ($MaxFailures -gt 0 -and $failures -ge $MaxFailures) {
            Write-Host "Stopping after $failures failure(s) (-MaxFailures $MaxFailures)" -ForegroundColor Red
            break
        }
    }

    Write-Host ""
}

# -- Summary --------------------------------------------------

$totalElapsed = (Get-Date) - $startTime
$passed = @($results | Where-Object { $_.Status -eq 'PASS' }).Count
$skipped = @($results | Where-Object { $_.Status -eq 'SKIP' }).Count
$changedCount = @($results | Where-Object { $_.Changed -eq '*' }).Count

Write-Host ""
Write-Host "============================================================" -ForegroundColor White
Write-Host "  RESULTS: $passed/$($results.Count) passed, $skipped skipped, $failures failed" -ForegroundColor $(if ($failures -eq 0) { 'Green' } else { 'Red' })
if ($changedCount -gt 0) {
    Write-Host "  Spec results changed: $changedCount" -ForegroundColor Yellow
}
Write-Host "  Total time: $( '{0:hh\:mm\:ss}' -f $totalElapsed )" -ForegroundColor White
Write-Host "============================================================" -ForegroundColor White
Write-Host ""

$results | Format-Table Source, Status, Prior, Saved, Changed, Attempts, Time, ExitCode -AutoSize

# Final cleanup: ensure app is stopped
Adb -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) | Out-Null

# Exit code = number of failures
exit $failures
