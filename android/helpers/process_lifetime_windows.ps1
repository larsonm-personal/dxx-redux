#!/usr/bin/env pwsh
# One job-owning process per worker, with inherited stdout/stderr handles
param([Parameter(Mandatory)][string]$Payload)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'process_lifetime.ps1')
Initialize-RegressionProcessLifetime
$command = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($Payload)) | ConvertFrom-Json
$start = [Diagnostics.ProcessStartInfo]::new([string]$command.FileName)
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
if ($command.Arguments) {
    $start.Arguments = [string]$command.Arguments
} else {
    foreach ($argument in $command.ArgumentList) { $start.ArgumentList.Add([string]$argument) }
}
$process = [Diagnostics.Process]::Start($start)
try {
    $process.WaitForExit()
    $exitCode = $process.ExitCode
} finally {
    $process.Dispose()
}
# Exiting closes the sole job handle, killing any remaining descendants
exit $exitCode
