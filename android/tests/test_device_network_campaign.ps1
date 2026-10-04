#!/usr/bin/env pwsh
# Explicit physical-device campaign; requires provisioned diagnostic apps and owned data
param(
    [string]$HostSerial,
    [string]$ClientSerial,
    [string]$OutputDirectory,
    [string[]]$CasePatterns = @('*baseline'),
    [ValidateRange(1, 100)][int]$Repeat = 1,
    [switch]$Reverse,
    [switch]$StopOnFailure,
    [switch]$List
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$arguments = @((Join-Path $PSScriptRoot 'run_device_network_campaign.py'))
foreach ($pattern in $CasePatterns) { $arguments += @('--case', $pattern) }
if ($List) {
    $arguments += '--list'
} else {
    if (-not $HostSerial -or -not $ClientSerial -or -not $OutputDirectory) {
        throw 'Select two physical serials and an output directory under android/temp'
    }
    if ($HostSerial -eq $ClientSerial -or $HostSerial -like 'emulator-*' -or $ClientSerial -like 'emulator-*') {
        throw 'Select two distinct physical devices'
    }
    $arguments += @('--host', $HostSerial, '--client', $ClientSerial, '--output', $OutputDirectory, '--repeat', $Repeat)
    if ($Reverse) { $arguments += '--reverse' }
    if ($StopOnFailure) { $arguments += '--stop-on-failure' }
}
& python @arguments
exit $LASTEXITCODE
