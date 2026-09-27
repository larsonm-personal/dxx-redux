#!/usr/bin/env pwsh
# Controlled startup races without launching or killing real processes
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"

function Test-DeviceOnline { param([string]$Serial) return $script:online }
function Get-ManagedEmulatorProcesses {
    param([string]$Serial)
    if ($script:existing) { [pscustomobject]@{ ProcessId = 123 } }
}
function Wait-EmulatorBootComplete {
    param([string]$Serial, [int]$TimeoutSeconds)
    $script:events.Add("wait:${Serial}:$TimeoutSeconds")
    return $script:ready.Dequeue()
}
function Stop-ManagedEmulator {
    param([string]$Serial)
    $script:events.Add("stop:$Serial")
    if ($script:stopWorks) { $script:existing = $false; $script:online = $false }
    return $script:stopWorks
}
function Ensure-ManagedAvdExists { param([string]$AvdName) return $true }
function Test-EmulatorAccelerationAvailable { return $true }
function Start-ManagedEmulatorProcess {
    param([string]$AvdName, [string]$Serial, [string]$GpuRenderer, [switch]$Headless)
    $script:events.Add("launch:$Serial")
    $script:online = $true
    return $true
}

# Use this known file for the executable-existence check; launch is mocked
$script:EMULATOR_EXE = $PSCommandPath
foreach ($case in @(
        @{ Name = 'healthy'; Online = $true; Existing = $true; Ready = @($true); Stop = $true; Result = $true; Events = 'wait:emulator-5554:240' },
        @{ Name = 'reconnect race'; Online = $false; Existing = $true; Ready = @($true); Stop = $true; Result = $true; Events = 'wait:emulator-5554:30' },
        @{ Name = 'stale process'; Online = $false; Existing = $true; Ready = @($false, $true); Stop = $true; Result = $true; Events = 'wait:emulator-5554:30,stop:emulator-5554,launch:emulator-5554,wait:emulator-5554:240' },
        @{ Name = 'shutdown failure'; Online = $false; Existing = $true; Ready = @($false); Stop = $false; Result = $false; Events = 'wait:emulator-5554:30,stop:emulator-5554' },
        @{ Name = 'boot failure'; Online = $true; Existing = $true; Ready = @($false, $true); Stop = $true; Result = $true; Events = 'wait:emulator-5554:240,stop:emulator-5554,launch:emulator-5554,wait:emulator-5554:240' },
        @{ Name = 'absent emulator'; Online = $false; Existing = $false; Ready = @($true); Stop = $true; Result = $true; Events = 'launch:emulator-5554,wait:emulator-5554:240' }
    )) {
    $script:online = $case.Online
    $script:existing = $case.Existing
    $script:stopWorks = $case.Stop
    $script:ready = [System.Collections.Queue]::new()
    foreach ($value in $case.Ready) { $script:ready.Enqueue($value) }
    $script:events = [System.Collections.Generic.List[string]]::new()
    $result = Start-ManagedEmulator -Serial emulator-5554 -AvdName test
    if ($result -ne $case.Result -or ($script:events -join ',') -cne $case.Events) {
        throw "$($case.Name): result=$result events=$($script:events -join ',')"
    }
    Write-Host "PASS: $($case.Name)"
}
