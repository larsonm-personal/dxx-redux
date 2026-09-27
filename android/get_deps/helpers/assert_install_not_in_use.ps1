#!/usr/bin/env pwsh
# Best-effort preflight for Windows executable locks before an expensive download
param([Parameter(Mandatory = $true)][string]$InstallDirectory)

$ErrorActionPreference = 'Stop'
$prefix = [IO.Path]::GetFullPath($InstallDirectory).TrimEnd('\', '/') + '\'
$users = @(Get-Process | Where-Object {
        $executablePath = $_.Path
        $executablePath -and $executablePath.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)
    })
if ($users.Count -gt 0) {
    [Console]::Error.WriteLine("ERROR: installation is in use: $InstallDirectory")
    foreach ($process in $users) {
        [Console]::Error.WriteLine("  $($process.ProcessName) (PID $($process.Id)): $($process.Path)")
    }
    [Console]::Error.WriteLine('Close the listed applications and retry; no processes were stopped and the existing installation was kept')
    exit 1
}
