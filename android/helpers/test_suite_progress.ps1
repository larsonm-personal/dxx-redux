#!/usr/bin/env pwsh

function Get-TestSuiteRemainingEstimate {
    param(
        [Parameter(Mandatory)][object[]]$Tests,
        [Parameter(Mandatory)][int]$CurrentProgressIndex
    )

    $totalSeconds = [int](($Tests |
                ForEach-Object { [int]$_.EstimatedRuntime } |
                Measure-Object -Sum).Sum)
    $remainingSeconds = [int](($Tests |
                Where-Object { [int]$_.ProgressIndex -ge $CurrentProgressIndex } |
                ForEach-Object { [int]$_.EstimatedRuntime } |
                Measure-Object -Sum).Sum)
    $remainingPercent = if ($totalSeconds -gt 0) {
        [int][Math]::Round(
            100 * $remainingSeconds / $totalSeconds,
            [MidpointRounding]::AwayFromZero
        )
    } else {
        0
    }

    return [pscustomobject]@{
        Seconds = $remainingSeconds
        Percent = $remainingPercent
    }
}
