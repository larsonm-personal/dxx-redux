# stop-stale-formatters.ps1 -- List or kill stale file-mutating formatter tasks
# Usage:
#   .\stop-stale-formatters.ps1        # list matching processes
#   .\stop-stale-formatters.ps1 -Kill  # kill matching process trees

param(
    [switch]$Kill
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot
$selfPid = $PID
$patterns = @(
    'run-code-quality.ps1',
    'run-clang-format.ps1',
    'run-ktlint.ps1',
    'run-psscriptanalyzer.ps1',
    'run-shfmt.ps1',
    'ktlint.jar',
    'clang-format'
)

$targets = Get-CimInstance Win32_Process | Where-Object {
    $_.ProcessId -ne $selfPid -and
    $_.CommandLine -and
    $_.CommandLine.Contains($repoRoot) -and
    ($patterns | Where-Object { $_.CommandLine.IndexOf($_, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 }).Count -gt 0
} | Sort-Object ProcessId -Unique

if ($targets.Count -eq 0) {
    Write-Host "No stale formatter tasks found"
    exit 0
}

Write-Host "Matching formatter tasks"
foreach ($target in $targets) {
    $started = if ($target.CreationDate) {
        [Management.ManagementDateTimeConverter]::ToDateTime($target.CreationDate)
    } else {
        $null
    }
    $age = if ($started) {
        [int](New-TimeSpan -Start $started -End (Get-Date)).TotalMinutes
    } else {
        -1
    }
    Write-Host "PID=$($target.ProcessId) Name=$($target.Name) AgeMin=$age"
    Write-Host "  $($target.CommandLine)"
}

if (-not $Kill) {
    Write-Host "Run with -Kill to stop these process trees"
    exit 0
}

foreach ($target in $targets) {
    Write-Host "Stopping PID $($target.ProcessId)"
    & taskkill /PID $target.ProcessId /T /F | Out-Null
}

Write-Host "Requested stop for matching formatter tasks"