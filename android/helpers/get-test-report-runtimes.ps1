#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Reads per-test runtimes from the most recent completed test report.
.DESCRIPTION
    Finds the newest report_*.md in ReportDir and emits one object per timed
    PASS, FAIL, or TIMEOUT result. Two-part times are interpreted as mm:ss;
    three-part times are interpreted as hh:mm:ss.
#>
param(
    [string]$ReportDir = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'temp\test_reports'),
    [string]$ExcludeReportPath
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
$report = Get-ChildItem -LiteralPath $ReportDir -Filter 'report_*.md' -File -ErrorAction SilentlyContinue |
    Where-Object { -not $excludedFullPath -or $_.FullName -ne $excludedFullPath } |
    Sort-Object LastWriteTimeUtc, Name -Descending |
    Select-Object -First 1
if (-not $report) {
    return
}

$resultPattern = '^\|\s*(?:PASS|FAIL|TIMEOUT)\s*\|\s*(?<time>\d+:\d{2}(?::\d{2})?)\s*\|\s*(?<name>[^|]+?)\s*\|'
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
    [pscustomobject]@{
        Name = $matches.name.Trim()
        Seconds = $seconds
        Elapsed = $matches.time
        ReportPath = $report.FullName
    }
}
