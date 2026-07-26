#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"

. "$PSScriptRoot\..\helpers\test_suite_progress.ps1"

$tests = @(
    @{ ProgressIndex = 1; EstimatedRuntime = 10 },
    @{ ProgressIndex = 2; EstimatedRuntime = 20 },
    @{ ProgressIndex = 3; EstimatedRuntime = 30 }
)

$atStart = Get-TestSuiteRemainingEstimate -Tests $tests -CurrentProgressIndex 1
if ($atStart.Seconds -ne 60 -or $atStart.Percent -ne 100) {
    throw "Start estimate was $($atStart.Seconds)s/$($atStart.Percent)%, expected 60s/100%"
}

# Actual elapsed time is deliberately absent from the calculation. Even when
# earlier tests overrun, queued tests retain their complete historical weight.
$afterOverrun = Get-TestSuiteRemainingEstimate -Tests $tests -CurrentProgressIndex 2
if ($afterOverrun.Seconds -ne 50 -or $afterOverrun.Percent -ne 83) {
    throw "Overrun estimate was $($afterOverrun.Seconds)s/$($afterOverrun.Percent)%, expected 50s/83%"
}

$atLastTest = Get-TestSuiteRemainingEstimate -Tests $tests -CurrentProgressIndex 3
if ($atLastTest.Seconds -ne 30 -or $atLastTest.Percent -ne 50) {
    throw "Last-test estimate was $($atLastTest.Seconds)s/$($atLastTest.Percent)%, expected 30s/50%"
}

Write-Host "PASS"
