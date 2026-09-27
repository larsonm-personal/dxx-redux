#!/usr/bin/env pwsh
# Preview by default; -Kill stops matched formatter trees for this checkout
param([switch]$Kill, [string]$RepositoryRoot = (Split-Path (Split-Path $PSScriptRoot)))
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'host_processes.ps1')
. (Join-Path $PSScriptRoot 'formatter_processes.ps1')
. (Join-Path $PSScriptRoot 'process_lifetime.ps1')
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]@('/', '\'))
$records = @(Get-DxxHostProcessInventory -IncludePaths)
$excluded = [Collections.Generic.HashSet[int]]::new()
$ancestorId = $PID
while ($ancestorId -gt 0 -and $excluded.Add($ancestorId)) {
    $ancestor = $records | Where-Object ProcessId -eq $ancestorId | Select-Object -First 1
    if (-not $ancestor) { break }
    $ancestorId = [int]$ancestor.ParentProcessId
}
$lock = $null
$lockPath = Join-Path $RepositoryRoot 'android/temp/run-code-quality.lock.json'
if (Test-Path -LiteralPath $lockPath -PathType Leaf) {
    try {
        $candidate = Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json
        if ($candidate.PSObject.Properties['process_start_ticks'] -and $candidate.PSObject.Properties['repository_root']) { $lock = $candidate }
    } catch { Write-Verbose "Ignoring unreadable formatter lock: $_" }
}
$targets = @()
foreach ($record in $records) {
    if ($excluded.Contains([int]$record.ProcessId)) { continue }
    $process = Get-Process -Id $record.ProcessId -ErrorAction SilentlyContinue
    if (-not $process) { continue }
    try {
        $ticks = $process.StartTime.ToUniversalTime().Ticks.ToString()
        if (Test-DxxFormatterProcess -Record $record -RepositoryRoot $RepositoryRoot -Lock $lock -StartTicks $ticks) {
            $targets += [pscustomobject]@{ Record = $record; StartTicks = $ticks }
        }
    } catch { Write-Verbose "Process $($record.ProcessId) exited or is not inspectable" }
    finally { $process.Dispose() }
}
if (-not $targets.Count) { Write-Host 'No matching formatter tasks found'; exit 0 }
foreach ($target in $targets) {
    Write-Host "PID=$($target.Record.ProcessId) Name=$($target.Record.Name)"
    Write-Host "  $($target.Record.CommandLine)"
}
if (-not $Kill) { Write-Host 'Run with -Kill to stop these process trees'; exit 0 }
$freshRecords = @(Get-DxxHostProcessInventory -IncludePaths)
foreach ($target in $targets) {
    $currentRecord = $freshRecords | Where-Object ProcessId -eq $target.Record.ProcessId | Select-Object -First 1
    if (-not $currentRecord) { continue }
    $process = Get-Process -Id $target.Record.ProcessId -ErrorAction SilentlyContinue
    if (-not $process) { continue }
    try {
        if ($process.StartTime.ToUniversalTime().Ticks.ToString() -cne $target.StartTicks) { continue }
        if (-not (Test-DxxFormatterProcess -Record $currentRecord -RepositoryRoot $RepositoryRoot -Lock $lock -StartTicks $target.StartTicks)) { continue }
        Write-Host "Stopping PID $($process.Id)"
        Stop-RegressionChildProcess -Process $process
    } finally { $process.Dispose() }
}
Write-Host 'Matched formatter processes have exited'
