#!/usr/bin/env pwsh

function Get-RemainingRuntimeEstimate {
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

function Get-TestSuiteRemainingEstimate {
    param(
        [Parameter(Mandatory)][object[]]$Tests,
        [Parameter(Mandatory)][int]$CurrentProgressIndex
    )

    return Get-RemainingRuntimeEstimate -Tests $Tests -CurrentProgressIndex $CurrentProgressIndex
}

function Format-RunnerDurationEstimate {
    param([Parameter(Mandatory)][double]$Seconds)

    $minutes = [int][Math]::Ceiling([Math]::Max(0, $Seconds) / 60.0)
    return ("{0:00}:{1:00}" -f [Math]::Floor($minutes / 60), ($minutes % 60))
}
