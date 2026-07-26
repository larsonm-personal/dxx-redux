#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Reads median per-test runtimes from recent completed test reports.
.DESCRIPTION
    Finds recent report_*.md files in ReportDir and emits one object per timed
    PASS, FAIL, or TIMEOUT result. Using the median prevents a single stalled
    test or infrastructure recovery from distorting the next suite estimate.
    Two-part times are interpreted as mm:ss; three-part times are interpreted
    as hh:mm:ss.
#>
param(
    [string]$ReportDir = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'temp\test_reports'),
    [string]$ExcludeReportPath,
    [ValidateRange(1, 20)]
    [int]$MaxReports = 4
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ReportDir -PathType Container)) {
    return
}

$excludedFullPath = if ($ExcludeReportPath) {
    [System.IO.Path]::GetFullPath($ExcludeReportPath)
} else {
    ''
}
$reports = @(Get-ChildItem -LiteralPath $ReportDir -Filter 'report_*.md' -File -ErrorAction SilentlyContinue |
        Where-Object { -not $excludedFullPath -or $_.FullName -ne $excludedFullPath } |
        Sort-Object LastWriteTimeUtc, Name -Descending |
        Select-Object -First $MaxReports)
if ($reports.Count -eq 0) {
    return
}

$resultPattern = '^\|\s*(?:PASS|FAIL|TIMEOUT)\s*\|\s*(?<time>\d+:\d{2}(?::\d{2})?)\s*\|\s*(?<name>[^|]+?)\s*\|'
$samplesByName = @{}
foreach ($report in $reports) {
    foreach ($line in Get-Content -LiteralPath $report.FullName) {
        if ($line -notmatch $resultPattern) {
            continue
        }

        $parts = @($matches.time -split ':' | ForEach-Object { [int]$_ })
        $seconds = if ($parts.Count -eq 3) {
            $parts[0] * 3600 + $parts[1] * 60 + $parts[2]
        } else {
            $parts[0] * 60 + $parts[1]
        }
        $name = $matches.name.Trim()
        if (-not $samplesByName.ContainsKey($name)) {
            $samplesByName[$name] = @()
        }
        $samplesByName[$name] += $seconds
    }
}

foreach ($name in @($samplesByName.Keys | Sort-Object)) {
    $samples = @($samplesByName[$name] | Sort-Object)
    $middle = [int][Math]::Floor($samples.Count / 2)
    $seconds = if (($samples.Count % 2) -eq 0) {
        [int][Math]::Round(
            ($samples[$middle - 1] + $samples[$middle]) / 2,
            [MidpointRounding]::AwayFromZero
        )
    } else {
        [int]$samples[$middle]
    }
    [pscustomobject]@{
        Name = $name
        Seconds = $seconds
        SampleCount = $samples.Count
        SourceReportCount = $reports.Count
        ReportPath = $reports[0].FullName
    }
}
