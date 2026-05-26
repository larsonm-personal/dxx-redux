#!/usr/bin/env pwsh
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
    'run-shellcheck.ps1',
    'run-shfmt.ps1',
    'ktlint.jar',
    'clang-format',
    'shellcheck',
    'shfmt'
)

function Get-ProcessCommandLineRecords {
    if (Get-Command Get-CimInstance -ErrorAction SilentlyContinue) {
        return @(Get-CimInstance Win32_Process | ForEach-Object {
                [pscustomobject]@{
                    ProcessId    = $_.ProcessId
                    Name         = $_.Name
                    CommandLine  = $_.CommandLine
                    CreationDate = $_.CreationDate
                }
            })
    }

    if (Test-Path -LiteralPath "/proc") {
        return @(Get-ChildItem -LiteralPath "/proc" -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^\d+$' } |
                ForEach-Object {
                    $processId = [int]$_.Name
                    $cmdlinePath = Join-Path $_.FullName "cmdline"
                    if (-not (Test-Path -LiteralPath $cmdlinePath)) {
                        return
                    }

                    $commandLine = [System.IO.File]::ReadAllText($cmdlinePath).Replace([char]0, ' ').Trim()
                    if (-not $commandLine) {
                        return
                    }

                    $proc = Get-Process -Id $processId -ErrorAction SilentlyContinue
                    [pscustomobject]@{
                        ProcessId    = $processId
                        Name         = if ($proc) { $proc.ProcessName } else { "" }
                        CommandLine  = $commandLine
                        CreationDate = if ($proc) { $proc.StartTime } else { $null }
                    }
                })
    }

    return @()
}

function Get-ChildProcessIds {
    param([int]$ParentProcessId)

    if (Get-Command Get-CimInstance -ErrorAction SilentlyContinue) {
        return @(Get-CimInstance Win32_Process -Filter "ParentProcessId = $ParentProcessId" |
                ForEach-Object { [int]$_.ProcessId })
    }

    if (-not (Test-Path -LiteralPath "/proc")) {
        return @()
    }

    return @(Get-ChildItem -LiteralPath "/proc" -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d+$' } |
            ForEach-Object {
                $statPath = Join-Path $_.FullName "stat"
                if (-not (Test-Path -LiteralPath $statPath)) {
                    return
                }
                $stat = [System.IO.File]::ReadAllText($statPath)
                if ($stat -match '\)\s+\S+\s+(\d+)') {
                    $ppid = [int]$Matches[1]
                    if ($ppid -eq $ParentProcessId) {
                        [int]$_.Name
                    }
                }
            })
}

function Stop-ProcessTree {
    param([int]$RootProcessId)

    foreach ($childPid in Get-ChildProcessIds -ParentProcessId $RootProcessId) {
        Stop-ProcessTree -RootProcessId $childPid
    }

    Stop-Process -Id $RootProcessId -Force -ErrorAction SilentlyContinue
}

$targets = Get-ProcessCommandLineRecords | Where-Object {
    $commandLine = $_.CommandLine
    $_.ProcessId -ne $selfPid -and
    $commandLine -and
    $commandLine.Contains($repoRoot) -and
    ($patterns | Where-Object { $commandLine.IndexOf($_, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 }).Count -gt 0
} | Sort-Object ProcessId -Unique

if ($targets.Count -eq 0) {
    Write-Host "No stale formatter tasks found"
    exit 0
}

Write-Host "Matching formatter tasks"
foreach ($target in $targets) {
    $started = if ($target.CreationDate -is [datetime]) {
        $target.CreationDate
    } elseif ($target.CreationDate) {
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
    if ($IsWindows -and (Get-Command taskkill -ErrorAction SilentlyContinue)) {
        & taskkill /PID $target.ProcessId /T /F | Out-Null
    } else {
        Stop-ProcessTree -RootProcessId $target.ProcessId
    }
}

Write-Host "Requested stop for matching formatter tasks"
