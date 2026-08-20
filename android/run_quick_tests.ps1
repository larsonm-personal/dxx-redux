#!/usr/bin/env pwsh
<#!
.SYNOPSIS
    Run a small fast smoke suite that should fit within about 3 minutes

.DESCRIPTION
    Runs a curated subset of host and single-emulator tests with fixed
    historical durations stored beside each test entry

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
. (Join-Path $helpersDir "test_helpers.ps1")
. (Join-Path (Join-Path $scriptDir "tests") "input_demo_host_build_guard.ps1")
. (Join-Path (Join-Path $scriptDir "tests") "input_demo_graphics_canary_helpers.ps1")

if (-not $ReportDir) {
    $ReportDir = Join-Path $repoRoot "temp\test_reports"
}
New-Item -Path $ReportDir -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$reportFile = Join-Path $ReportDir "quick_report_$timestamp.md"
$runTestScript = Join-Path $helpersDir "run_test.ps1"

function New-QuickDemoSubset {
    param(
        [string]$DestinationRoot,
        $Canary
    )

    if (Test-Path -LiteralPath $DestinationRoot) {
        Remove-Item -LiteralPath $DestinationRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null

    $sourcePath = Resolve-InputDemoGraphicsCanaryPath `
        -DemoRoot (Join-Path $scriptDir "regression_demos") `
        -Entry $Canary
    Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $DestinationRoot $Canary.FileName)

    return $DestinationRoot
}

$inputDemoCanaryManifest = Read-InputDemoGraphicsCanaryManifest `
    -ManifestPath (Join-Path $scriptDir "tests\input_demo_graphics_canaries.txt")
$quickDemoRoot = New-QuickDemoSubset `
    -DestinationRoot (Join-Path $repoRoot "temp\quick_demo_subset_$timestamp") `
    -Canary $inputDemoCanaryManifest['d2']
$quickPrimaryResultRoot = Join-Path $repoRoot "temp\quick_demo_primary_results_$timestamp"
$headlessExecutable = Get-InputDemoExecutablePath -RepoRoot $repoRoot -GameName "d2" -PreferHeadlessConsole
$headlessFreshnessIssue = Get-InputDemoExecutableFreshnessIssue `
    -RepoRoot $repoRoot `
    -GameName "d2" `
    -ExecutablePath $headlessExecutable `
    -Description "D2 headless replay executable"
$headlessHistoricalSeconds = if ($headlessFreshnessIssue) { 65 } else { 4 }

$quickTests = @(
    @{ Name = "test_cue_iso"; Type = "ps1"; RelativePath = "tests\test_cue_iso.ps1"; HistoricalSeconds = 5 },
    @{ Name = "test_fpcalc_and_acoustid"; Type = "ps1"; RelativePath = "tests\test_fpcalc_and_acoustid.ps1"; HistoricalSeconds = 3 },
    @{ Name = "test_input_demo_rng_trace_compare"; Type = "ps1"; RelativePath = "tests\test_input_demo_rng_trace_compare.ps1"; HistoricalSeconds = 0 },
    @{ Name = "test_input_demo_state_trace_compare"; Type = "ps1"; RelativePath = "tests\test_input_demo_state_trace_compare.ps1"; HistoricalSeconds = 0 },
    @{ Name = "test_input_demo_regressions_headless_quick"; Type = "ps1"; RelativePath = "tests\test_input_demo_regressions.ps1"; Arguments = @("-DemoRoot", $quickDemoRoot, "-Game", "d2", "-TimeoutSeconds", "120", "-StopOnFirstFailure", "-ResultArchiveRoot", $quickPrimaryResultRoot); HistoricalSeconds = $headlessHistoricalSeconds },
    @{ Name = "test_input_demo_regressions_graphics_quick"; Type = "ps1"; RelativePath = "tests\test_input_demo_regressions_graphics.ps1"; Arguments = @("-DemoRoot", $quickDemoRoot, "-Game", "d2", "-TimeoutSeconds", "120", "-StopOnFirstFailure", "-ReferenceResultRoot", $quickPrimaryResultRoot); HistoricalSeconds = 10 },
    @{ Name = "test_quick_record_classic_sidecar"; Type = "jsonc"; RelativePath = "game_scripts\test_quick_record_classic_sidecar.jsonc"; HistoricalSeconds = 35 },
    @{ Name = "test_mod_loading"; Type = "jsonc"; RelativePath = "game_scripts\test_mod_loading.jsonc"; HistoricalSeconds = 25 },
    @{ Name = "test_launcher_graphics_debug_prefs"; Type = "jsonc"; RelativePath = "game_scripts\test_launcher_graphics_debug_prefs.jsonc"; HistoricalSeconds = 27 },
    @{ Name = "test_menu_scale_d2"; Type = "jsonc"; RelativePath = "game_scripts\test_menu_scale_d2.jsonc"; HistoricalSeconds = 20 },
    @{ Name = "test_levelcomplete_touch_skip"; Type = "jsonc"; RelativePath = "game_scripts\test_levelcomplete_touch_skip.jsonc"; HistoricalSeconds = 18 }
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

function Get-QuickTestHistoricalSeconds {
    param([hashtable]$Test)

    if (-not $Test -or -not $Test.ContainsKey("HistoricalSeconds") -or $null -eq $Test.HistoricalSeconds) {
        return 0
    }

    return [int]$Test.HistoricalSeconds
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
    $historicalSeconds = Get-QuickTestHistoricalSeconds -Test $Test
    $logFile = Join-Path $ReportDir ("{0}_{1}.log" -f $name, $timestamp)

    Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
    Write-Host "  Running: $name [$($Test.Type)]  (timeout: ${TestTimeoutSeconds}s)" -ForegroundColor White

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $psScript = $null
    $psArguments = @()

    if ($Test.Type -eq "jsonc") {
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
            Historical = Format-SecondsAsClock -Seconds $historicalSeconds
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
    $stdout = ""
    $stderr = ""
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
    $skipMarker = [regex]::Match("$stdout`n$stderr", '(?m)^RESULT:\s*SKIP(?:\s*\((?<reason>[^\r\n]*)\))?')
    $skipDeclared = $skipMarker.Success
    $status = Get-TestStatusFromExitCode -ExitCode $exitCode -TimedOut $timedOut -SkipDeclared $skipDeclared
    $color = switch ($status) {
        "PASS" { "Green" }
        "SKIP" { "DarkYellow" }
        "TIMEOUT" { "Yellow" }
        default { "Red" }
    }

    Write-Host "  $status ($elapsed)" -ForegroundColor $color

    return @{
        Name = $name
        Type = $Test.Type
        Status = $status
        ExitCode = $exitCode
        Elapsed = $elapsed
        LogFile = $logFile
        Historical = Format-SecondsAsClock -Seconds $historicalSeconds
        Reason = if ($status -eq "SKIP" -and $skipMarker.Groups["reason"].Success) {
            $skipMarker.Groups["reason"].Value
        } else { "" }
    }
}

$historicalTotalSeconds = [int](($quickTests | ForEach-Object { Get-QuickTestHistoricalSeconds -Test $_ } | Measure-Object -Sum).Sum)
$historicalTotal = Format-SecondsAsClock -Seconds $historicalTotalSeconds
$historicalEstimateSource = if ($headlessFreshnessIssue) {
    "conservative per-test baselines with a required headless rebuild"
} else {
    "conservative per-test baselines"
}

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  DXX-Redux Quick Test Suite" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tests selected: $($quickTests.Count)" -ForegroundColor White
Write-Host "Historical estimate: $historicalTotal from $historicalEstimateSource" -ForegroundColor White
Write-Host "Budget: $(Format-SecondsAsClock -Seconds $MaxTotalSeconds)" -ForegroundColor White
Write-Host ""

if ($historicalTotalSeconds -gt $MaxTotalSeconds) {
    Write-Host "Estimate exceeds budget; lower-priority cases will be skipped as the deadline approaches" -ForegroundColor Yellow
}

$results = @()
$skipped = @()
$passCount = 0
$failCount = 0
$timeoutCount = 0
$runtimeSkipCount = 0
$totalSw = [System.Diagnostics.Stopwatch]::StartNew()

foreach ($test in $quickTests) {
    $elapsedSeconds = [int][Math]::Floor($totalSw.Elapsed.TotalSeconds)
    $remainingSeconds = $MaxTotalSeconds - $elapsedSeconds
    $historicalSeconds = Get-QuickTestHistoricalSeconds -Test $test
    if ($remainingSeconds -lt $historicalSeconds) {
        Write-Host "Skipping $($test.Name) because only $(Format-SecondsAsClock -Seconds ([Math]::Max(0, $remainingSeconds))) remains in the budget" -ForegroundColor Yellow
        $skipped += @{
            Name = $test.Name
            Type = $test.Type
            Reason = "budget"
            Historical = Format-SecondsAsClock -Seconds $historicalSeconds
        }
        continue
    }

    $result = Invoke-QuickTest -Test $test
    $results += $result

    switch ($result.Status) {
        "PASS" { $passCount++ }
        "SKIP" { $runtimeSkipCount++ }
        "TIMEOUT" { $timeoutCount++ }
        default { $failCount++ }
    }

    if ($StopOnFail -and $result.Status -in @("FAIL", "TIMEOUT")) {
        Write-Host "Stopping early because -StopOnFail was set" -ForegroundColor Yellow
        break
    }
}

$totalSw.Stop()
$totalElapsed = $totalSw.Elapsed.ToString("hh\:mm\:ss")
$totalSkipped = $runtimeSkipCount + $skipped.Count
$accountedTestNames = [System.Collections.Generic.HashSet[string]]::new()
foreach ($result in $results) { [void]$accountedTestNames.Add([string]$result.Name) }
foreach ($skippedTest in $skipped) { [void]$accountedTestNames.Add([string]$skippedTest.Name) }
$notRun = @(
    $quickTests |
        Where-Object { -not $accountedTestNames.Contains([string]$_.Name) } |
        ForEach-Object {
            @{
                Name = $_.Name
                Type = $_.Type
                Reason = "stopped after prior failure"
                Historical = Format-SecondsAsClock -Seconds (Get-QuickTestHistoricalSeconds -Test $_)
            }
        }
)

Write-Host ""
Write-Host "Passed: $passCount  Failed: $failCount  Timeouts: $timeoutCount  Skipped: $totalSkipped  Not run: $($notRun.Count)  Total time: $totalElapsed" -ForegroundColor White
Write-Host "Report: $reportFile" -ForegroundColor Cyan
Write-Host ""

$md = @()
$md += "# Quick Test Report - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$md += ""
$md += "## Summary"
$md += "- Passed: $passCount"
$md += "- Failed: $failCount"
$md += "- Timeouts: $timeoutCount"
$md += "- Skipped: $totalSkipped"
$md += "- Not run: $($notRun.Count)"
$md += "- Total time: $totalElapsed"
$md += "- Budget: $(Format-SecondsAsClock -Seconds $MaxTotalSeconds)"
$md += "- Historical estimate: $historicalTotal from $historicalEstimateSource"
$md += ""
$md += "## Selected Tests"
$md += ""
$md += "| Historical | Test | Type |"
$md += "|-----------|------|------|"
foreach ($test in $quickTests) {
    $md += "| $(Format-SecondsAsClock -Seconds (Get-QuickTestHistoricalSeconds -Test $test)) | $($test.Name) | $($test.Type) |"
}
$md += ""
$md += "## Results"
$md += ""
$md += "| Status | Time | Historical | Test | Type |"
$md += "|--------|------|------------|------|------|"
foreach ($result in $results) {
    $resultType = if ($result.Status -eq "SKIP" -and $result.Reason) {
        "$($result.Type) ($($result.Reason))"
    } else { $result.Type }
    $md += "| $($result.Status) | $($result.Elapsed) | $($result.Historical) | $($result.Name) | $resultType |"
}
foreach ($item in $skipped) {
    $md += "| SKIP | -- | $($item.Historical) | $($item.Name) | $($item.Type) ($($item.Reason)) |"
}
foreach ($item in $notRun) {
    $md += "| NOT_RUN | -- | $($item.Historical) | $($item.Name) | $($item.Type) ($($item.Reason)) |"
}
$md += ""

if ($failCount -gt 0 -or $timeoutCount -gt 0 -or $notRun.Count -gt 0) {
    $md += "## Non-passing Results"
    $md += ""
    foreach ($result in ($results | Where-Object { $_.Status -in @('FAIL', 'TIMEOUT') })) {
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

$retentionArtifacts = @($reportFile, $quickDemoRoot, $quickPrimaryResultRoot) | Where-Object { Test-Path -LiteralPath $_ }
$retentionArtifacts += @(Get-ChildItem -LiteralPath $ReportDir -File | Where-Object { $_.Name -like "*_$timestamp.*" } | Select-Object -ExpandProperty FullName)
& (Join-Path $helpersDir "retain-recent-artifacts.ps1") -Artifacts $retentionArtifacts

if ($failCount -gt 0 -or $timeoutCount -gt 0 -or $notRun.Count -gt 0) {
    exit 1
}
exit 0
