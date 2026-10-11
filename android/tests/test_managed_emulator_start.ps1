#!/usr/bin/env pwsh
# Controlled startup races without launching or killing real processes
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$script:capturedLaunches = [Collections.Generic.List[object]]::new()
$script:launchProcess = $null

function Test-DeviceOnline { param([string]$Serial) return $script:online }
function Get-ManagedEmulatorProcesses {
    param([string]$Serial)
    if ($script:existing) {
        if ($script:launchProcess) { $script:launchProcess }
        else { [pscustomobject]@{ ProcessId = 123 } }
    }
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
    param([string]$AvdName, [string]$Serial, [string]$GpuRenderer, [switch]$Headless, [string]$DataPath, [switch]$ReadOnly)
    $script:events.Add("launch:$Serial")
    $script:capturedLaunches.Add(@{ AvdName = $AvdName; GpuRenderer = $GpuRenderer; Headless = [bool]$Headless; DataPath = $DataPath; ReadOnly = [bool]$ReadOnly })
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

foreach ($fixture in @(
        @{
            Process = [pscustomobject]@{ ProcessId = 123; Name = 'emulator.exe'; CommandLine = '"C:\Program Files\Android\emulator.exe" -avd fixture -port 5554 -gpu host -data "C:\Test Data\private.img" -no-window -read-only' }
            DataPath = 'C:\Test Data\private.img'
        },
        @{
            Process = [pscustomobject]@{ ProcessId = 123; Arguments = @('/sdk/emulator', '-avd', 'fixture', '-port', '5554', '-gpu', 'host', '-data', '/tmp/test data/private.img', '-no-window', '-read-only') }
            DataPath = '/tmp/test data/private.img'
        }
    )) {
    $script:existing = $true
    $script:launchProcess = $fixture.Process
    $settings = Get-ManagedEmulatorLaunchSettings -Serial emulator-5554
    if ($settings.AvdName -cne 'fixture' -or $settings.GpuRenderer -cne 'host' -or
        $settings.DataPath -cne $fixture.DataPath -or -not $settings.Headless -or -not $settings.ReadOnly) {
        throw "Recovery did not preserve the selected emulator configuration: $($settings | ConvertTo-Json -Compress)"
    }
    $script:existing = $false
    $script:online = $false
    $script:ready = [Collections.Queue]::new()
    # Exercise both the requested renderer and the fallback launch with the same private disk
    $script:ready.Enqueue($false)
    $script:ready.Enqueue($true)
    $script:capturedLaunches.Clear()
    if (-not (Start-ManagedEmulator -Serial emulator-5554 @settings)) { throw 'Configured recovery failed' }
    if ($script:capturedLaunches.Count -ne 2) { throw 'Expected primary and fallback launches' }
    foreach ($launch in $script:capturedLaunches) {
        if ($launch.AvdName -cne 'fixture' -or $launch.DataPath -cne $fixture.DataPath -or -not $launch.Headless -or -not $launch.ReadOnly) {
            throw 'Restart or renderer fallback changed the private disk or isolation flags'
        }
    }
    Write-Host "PASS: recovery and fallback preserve private data, headless and read-only settings ($($fixture.DataPath))"
}
