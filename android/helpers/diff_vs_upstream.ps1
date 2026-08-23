#!/usr/bin/env pwsh
# Produce a summary of the d1/ and d2/ diff vs a base branch.
# Default base is upstream/main. Writes sorted numstat and summary to temp/.
#
# Usage:
#   .\android\helpers\diff_vs_upstream.ps1
#   .\android\helpers\diff_vs_upstream.ps1 -Base origin/main
#   .\android\helpers\diff_vs_upstream.ps1 -Top 30
#   .\android\helpers\diff_vs_upstream.ps1 -ShowContent   # also dumps top-N full diffs

[CmdletBinding()]
param(
    [string]$Base = "upstream/main",
    [int]$Top = 40,
    [switch]$ShowContent
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$tempDir = Join-Path $repoRoot "temp"
if (-not (Test-Path $tempDir)) { New-Item -ItemType Directory -Path $tempDir | Out-Null }

$numstatPath = Join-Path $tempDir "d1d2_diff_numstat.txt"
$sortedPath = Join-Path $tempDir "d1d2_diff_sorted.txt"
$summaryPath = Join-Path $tempDir "d1d2_diff_summary.txt"

Write-Host "Base: $Base"
Write-Host "Collecting numstat for d1/ and d2/..."

$numstat = git -C $repoRoot diff --numstat $Base -- d1/ d2/
if ($LASTEXITCODE -ne 0) {
    Write-Error "git diff failed. Is '$Base' a valid ref?"
    exit 1
}

[IO.File]::WriteAllText($numstatPath, ($numstat -join [Environment]::NewLine) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))

$rows = foreach ($line in $numstat) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $parts = $line -split "`t"
    if ($parts.Count -lt 3) { continue }
    [pscustomobject]@{
        Added   = [int]$parts[0]
        Removed = [int]$parts[1]
        Total   = [int]$parts[0] + [int]$parts[1]
        Path    = $parts[2]
    }
}

$sorted = $rows | Sort-Object Total -Descending
[IO.File]::WriteAllText($sortedPath, ($sorted | Format-Table -AutoSize | Out-String), [Text.UTF8Encoding]::new($false))

$totalAdded = ($rows | Measure-Object Added   -Sum).Sum
$totalRemoved = ($rows | Measure-Object Removed -Sum).Sum
$fileCount = $rows.Count

$d1Files = $rows | Where-Object { $_.Path -like "d1/*" }
$d2Files = $rows | Where-Object { $_.Path -like "d2/*" }

$summary = @()
$summary += "d1/d2 diff vs $Base"
$summary += "  files:   $fileCount"
$summary += "  d1 files: $($d1Files.Count)"
$summary += "  d2 files: $($d2Files.Count)"
$summary += "  +added:  $totalAdded"
$summary += "  -removed: $totalRemoved"
$summary += ""
$summary += "Top $Top by total churn:"
$summary += ($sorted | Select-Object -First $Top | Format-Table Added, Removed, Total, Path -AutoSize | Out-String).TrimEnd()
[IO.File]::WriteAllText($summaryPath, ($summary -join [Environment]::NewLine) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))

Write-Host ""
Write-Host "Wrote:"
Write-Host "  $numstatPath"
Write-Host "  $sortedPath"
Write-Host "  $summaryPath"
Write-Host ""
Get-Content $summaryPath

if ($ShowContent) {
    $dumpPath = Join-Path $tempDir "d1d2_diff_top$Top.patch"
    $topPaths = $sorted | Select-Object -First $Top -ExpandProperty Path
    $patchText = git -C $repoRoot diff $Base -- $topPaths
    [IO.File]::WriteAllText($dumpPath, ($patchText -join [Environment]::NewLine) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
    Write-Host ""
    Write-Host "Wrote full patch for top $Top files: $dumpPath"
}
