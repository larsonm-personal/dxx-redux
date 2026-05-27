#!/usr/bin/env pwsh
<#!
.SYNOPSIS
    Run a small fast smoke suite that should fit within about 3 minutes

.DESCRIPTION
    Runs a curated subset of host and single-emulator tests selected from
    `temp\test_reports\report_20260518_223317.md`

    The quick suite keeps to about 3 minutes while including one committed
    regression demo in both headless and graphics replay modes. It
    intentionally avoids extract, server, and dual-emulator tiers whose setup
    cost would push the total wall-clock time past the quick-run budget

.PARAMETER ReportDir
    Directory for per-test logs and the markdown summary report

.PARAMETER StopOnFail
    Stop after the first failure or timeout

.PARAMETER MaxTotalSeconds
    Wall-clock budget used to decide whether there is enough room to start the
    next historically-timed test

.PARAMETER TestTimeoutSeconds
    Maximum seconds allowed for any single test process

.EXAMPLE
    .\run_quick_tests.ps1
    .\run_quick_tests.ps1 -StopOnFail
#>

param(
    [string]$ReportDir,
    [switch]$StopOnFail,
    [int]$MaxTotalSeconds = 180,
    [int]$TestTimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $PSCommandPath
$helpersDir = Join-Path $scriptDir "helpers"
$repoRoot = Split-Path $scriptDir

if (-not $ReportDir) {
    $ReportDir = Join-Path $repoRoot "temp\test_reports"
}
New-Item -Path $ReportDir -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$reportFile = Join-Path $ReportDir "quick_report_$timestamp.md"
$runTestScript = Join-Path $helpersDir "run_test.ps1"

function New-QuickDemoSubset {
    param([string]$DestinationRoot)

    $demoFileNames = @(
        "d2_descent2_level9_20260511_192533.dximdemo"
    )

    if (Test-Path -LiteralPath $DestinationRoot) {
        Remove-Item -LiteralPath $DestinationRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null

    foreach ($demoFileName in $demoFileNames) {
        $sourcePath = Join-Path $scriptDir (Join-Path "regression_demos" $demoFileName)
        if (-not (Test-Path -LiteralPath $sourcePath)) {
            throw "Quick demo source not found: $sourcePath"
        }
        Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $DestinationRoot $demoFileName)
    }

    return $DestinationRoot
}

$quickDemoRoot = New-QuickDemoSubset -DestinationRoot (Join-Path $repoRoot "temp\quick_demo_subset_$timestamp")

$quickTests = @(
    @{ Name = "test_cue_iso"; Type = "ps1"; RelativePath = "tests\test_cue_iso.ps1"; HistoricalSeconds = 3 },
    @{ Name = "test_fpcalc_and_acoustid"; Type = "ps1"; RelativePath = "tests\test_fpcalc_and_acoustid.ps1"; HistoricalSeconds = 2 },
    @{ Name = "test_input_demo_rng_trace_compare"; Type = "ps1"; RelativePath = "tests\test_input_demo_rng_trace_compare.ps1"; HistoricalSeconds = 0 },
    @{ Name = "test_input_demo_state_trace_compare"; Type = "ps1"; RelativePath = "tests\test_input_demo_state_trace_compare.ps1"; HistoricalSeconds = 0 },
    @{ Name = "test_input_demo_regressions_headless_quick"; Type = "ps1"; RelativePath = "tests\test_input_demo_regressions.ps1"; Arguments = @("-DemoRoot", $quickDemoRoot, "-Game", "d2", "-TimeoutSeconds", "120", "-StopOnFirstFailure"); HistoricalSeconds = 3 },
    @{ Name = "test_input_demo_regressions_graphics_quick"; Type = "ps1"; RelativePath = "tests\test_input_demo_regressions_graphics.ps1"; Arguments = @("-DemoRoot", $quickDemoRoot, "-Game", "d2", "-TimeoutSeconds", "120", "-StopOnFirstFailure"); HistoricalSeconds = 10 },
    @{ Name = "test_autoselect_plx"; Type = "ps1"; RelativePath = "tests\test_autoselect_plx.ps1"; HistoricalSeconds = 7 },
    @{ Name = "test_mod_loading"; Type = "json5"; RelativePath = "game_scripts\test_mod_loading.json5"; HistoricalSeconds = 1 },
    @{ Name = "test_menu_scale_d2"; Type = "json5"; RelativePath = "game_scripts\test_menu_scale_d2.json5"; HistoricalSeconds = 12 },
    @{ Name = "test_launcher_graphics_debug_prefs"; Type = "json5"; RelativePath = "game_scripts\test_launcher_graphics_debug_prefs.json5"; HistoricalSeconds = 15 },
    @{ Name = "test_quick_record_classic_sidecar_stage"; Type = "json5"; RelativePath = "game_scripts\test_quick_record_classic_sidecar_stage.json5"; HistoricalSeconds = 23 },
    @{ Name = "test_quick_record_classic_sidecar_install"; Type = "json5"; RelativePath = "game_scripts\test_quick_record_classic_sidecar_install.json5"; HistoricalSeconds = 18 },
    @{ Name = "test_launcher_dpad"; Type = "ps1"; RelativePath = "tests\test_launcher_dpad.ps1"; HistoricalSeconds = 16 },
    @{ Name = "test_saf_basic"; Type = "json5"; RelativePath = "game_scripts\test_saf_basic.json5"; HistoricalSeconds = 21 },
    @{ Name = "test_levelcomplete_touch_skip"; Type = "json5"; RelativePath = "game_scripts\test_levelcomplete_touch_skip.json5"; HistoricalSeconds = 17 }
)

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

function Format-SecondsAsClock {
    param([int]$Seconds)

    return ([TimeSpan]::FromSeconds($Seconds)).ToString("mm\:ss")
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
        'TIMEOUT',
        'FAIL:',
        'ASSERT_FAIL',
        'EXCEPTION:',
        'PASS for D[12]',
        'FAIL for D[12]',
        'script","status":"FAIL',
        'SetupActivity not responding',
        'Launcher recovery failed',
        'Emulator could not be restored'
    )

    if ($Status -eq 'TIMEOUT') {
        $patterns = @('timed out', 'TIMEOUT waiting') + $patterns
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
        return $lines | Select-Object -Last 15
    }

    return ($selectedIndexes | Sort-Object | ForEach-Object { $lines[$_] })
}

function Invoke-QuickTest {
    param([hashtable]$Test)

    $name = $Test.Name
    $logFile = Join-Path $ReportDir ("{0}_{1}.log" -f $name, $timestamp)

    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  Running: $name [$($Test.Type)]  (timeout: ${TestTimeoutSeconds}s)" -ForegroundColor White

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $psScript = $null
    $psArguments = @()

    if ($Test.Type -eq "json5") {
        $psScript = $runTestScript
        $psArguments = @("-ScriptName", [System.IO.Path]::GetFileName($Test.RelativePath))
        if ($Test.ContainsKey("Arguments")) {
            $psArguments += @($Test.Arguments)
        }
    } else {
        $psScript = Join-Path $scriptDir $Test.RelativePath
        if ($Test.ContainsKey("Arguments")) {
            $psArguments = @($Test.Arguments)
        }
    }

    if (-not (Test-Path -LiteralPath $psScript)) {
        "Missing test entry point: $psScript" | Out-File -FilePath $logFile -Encoding utf8
        Write-Host "  FAIL: Missing test entry point" -ForegroundColor Red
        return @{
            Name = $name
            Type = $Test.Type
            Status = "FAIL"
            ExitCode = 1
            Elapsed = "00:00"
            LogFile = $logFile
            Historical = Format-SecondsAsClock -Seconds $Test.HistoricalSeconds
        }
    }

    $quotedScript = if ($psScript -match '[\s"]') {
        '"' + ($psScript -replace '"', '\"') + '"'
    } else {
        $psScript
    }
    $argumentText = ConvertTo-ArgumentText -Arguments $psArguments

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "pwsh"
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

        if (-not $proc.WaitForExit($TestTimeoutSeconds * 1000)) {
            $timedOut = $true
            try { $proc.Kill($true) } catch { try { $proc.Kill() } catch {} }
            Start-Sleep -Seconds 1
        } else {
            $exitCode = $proc.ExitCode
        }

        $stdoutTask.Wait(5000) | Out-Null
        $stderrTask.Wait(5000) | Out-Null
        $stdout = if ($stdoutTask.IsCompleted) { $stdoutTask.Result } else { "" }
        $stderr = if ($stderrTask.IsCompleted) { $stderrTask.Result } else { "" }
        $proc.Dispose()

        $stdout | Out-File -FilePath $logFile -Encoding utf8
        if ($stderr) {
            $stderr | Out-File -FilePath $logFile -Append -Encoding utf8
        }
        if ($timedOut) {
            "TIMEOUT: Test killed after ${TestTimeoutSeconds}s" | Out-File -FilePath $logFile -Append -Encoding utf8
        }

        $lines = ($stdout -split "`n" | Where-Object { $_.Trim() })
        foreach ($line in ($lines | Select-Object -Last 5)) {
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
        $status = "TIMEOUT"
        $exitCode = 1
        $color = "Yellow"
    } elseif ($exitCode -eq 0) {
        $status = "PASS"
        $color = "Green"
    } else {
        $status = "FAIL"
        $color = "Red"
    }

    Write-Host "  $status ($elapsed)" -ForegroundColor $color

    return @{
        Name = $name
        Type = $Test.Type
        Status = $status
        ExitCode = $exitCode
        Elapsed = $elapsed
        LogFile = $logFile
        Historical = Format-SecondsAsClock -Seconds $Test.HistoricalSeconds
    }
}

$historicalTotalSeconds = [int](($quickTests | Measure-Object -Property HistoricalSeconds -Sum).Sum)
$historicalTotal = Format-SecondsAsClock -Seconds $historicalTotalSeconds

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  DXX-Redux Quick Test Suite" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tests selected: $($quickTests.Count)" -ForegroundColor White
Write-Host "Historical estimate: $historicalTotal from report_20260518_223317.md" -ForegroundColor White
Write-Host "Budget: $(Format-SecondsAsClock -Seconds $MaxTotalSeconds)" -ForegroundColor White
Write-Host ""

if ($historicalTotalSeconds -gt $MaxTotalSeconds) {
    Write-Host "Quick suite estimate exceeds budget" -ForegroundColor Red
    exit 1
}

$results = @()
$skipped = @()
$passCount = 0
$failCount = 0
$timeoutCount = 0
$totalSw = [System.Diagnostics.Stopwatch]::StartNew()

foreach ($test in $quickTests) {
    $elapsedSeconds = [int][Math]::Floor($totalSw.Elapsed.TotalSeconds)
    $remainingSeconds = $MaxTotalSeconds - $elapsedSeconds
    if ($remainingSeconds -lt $test.HistoricalSeconds) {
        Write-Host "Skipping $($test.Name) because only $(Format-SecondsAsClock -Seconds ([Math]::Max(0, $remainingSeconds))) remains in the budget" -ForegroundColor Yellow
        $skipped += @{
            Name = $test.Name
            Type = $test.Type
            Reason = "budget"
            Historical = Format-SecondsAsClock -Seconds $test.HistoricalSeconds
        }
        continue
    }

    $result = Invoke-QuickTest -Test $test
    $results += $result

    switch ($result.Status) {
        "PASS" { $passCount++ }
        "TIMEOUT" { $timeoutCount++; $failCount++ }
        default { $failCount++ }
    }

    if ($StopOnFail -and $result.ExitCode -ne 0) {
        Write-Host "Stopping early because -StopOnFail was set" -ForegroundColor Yellow
        break
    }
}

$totalSw.Stop()
$totalElapsed = $totalSw.Elapsed.ToString("hh\:mm\:ss")

Write-Host ""
Write-Host "Passed: $passCount  Failed: $failCount  Timeouts: $timeoutCount  Skipped: $($skipped.Count)  Total time: $totalElapsed" -ForegroundColor White
Write-Host "Report: $reportFile" -ForegroundColor Cyan
Write-Host ""

$md = @()
$md += "# Quick Test Report - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$md += ""
$md += "## Summary"
$md += "- Passed: $passCount"
$md += "- Failed: $failCount"
$md += "- Timeouts: $timeoutCount"
$md += "- Skipped: $($skipped.Count)"
$md += "- Total time: $totalElapsed"
$md += "- Budget: $(Format-SecondsAsClock -Seconds $MaxTotalSeconds)"
$md += "- Historical estimate: $historicalTotal"
$md += ""
$md += "## Selected Tests"
$md += ""
$md += "| Historical | Test | Type |"
$md += "|-----------|------|------|"
foreach ($test in $quickTests) {
    $md += "| $(Format-SecondsAsClock -Seconds $test.HistoricalSeconds) | $($test.Name) | $($test.Type) |"
}
$md += ""
$md += "## Results"
$md += ""
$md += "| Status | Time | Historical | Test | Type |"
$md += "|--------|------|------------|------|------|"
foreach ($result in $results) {
    $md += "| $($result.Status) | $($result.Elapsed) | $($result.Historical) | $($result.Name) | $($result.Type) |"
}
foreach ($item in $skipped) {
    $md += "| SKIP | -- | $($item.Historical) | $($item.Name) | $($item.Type) ($($item.Reason)) |"
}
$md += ""

if ($failCount -gt 0 -or $timeoutCount -gt 0) {
    $md += "## Non-passing Results"
    $md += ""
    foreach ($result in ($results | Where-Object { $_.Status -ne 'PASS' })) {
        $md += "### $($result.Name)"
        $md += "- Status: $($result.Status)"
        $md += "- Exit code: $($result.ExitCode)"
        $md += "- Log: ``$(Split-Path $result.LogFile -Leaf)``"
        $excerpt = Get-ReportLogExcerpt -LogFile $result.LogFile -Status $result.Status
        if ($excerpt.Count -gt 0) {
            $md += '```'
            $md += $excerpt
            $md += '```'
        }
        $md += ""
    }
}

$md -join "`n" | Set-Content -Path $reportFile -Encoding utf8

if ($failCount -gt 0) {
    exit 1
}
exit 0