#!/usr/bin/env pwsh
# Controlled startup races without launching or killing real processes
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$script:capturedLaunches = [Collections.Generic.List[object]]::new()
$script:launchProcess = $null

$startProcessImplementation = ${function:Start-ManagedEmulatorProcess}
$waitBootImplementation = ${function:Wait-EmulatorBootComplete}

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
function Test-EmulatorAccelerationAvailable { return $script:accelerationAvailable }
function Start-ManagedEmulatorProcess {
    param([string]$AvdName, [string]$Serial, [string]$GpuRenderer, [switch]$Headless, [string]$DataPath, [switch]$ReadOnly, [switch]$SoftwareCpu)
    $script:lastLaunch = @{ Renderer = $GpuRenderer; Headless = [bool]$Headless; SoftwareCpu = [bool]$SoftwareCpu }
    $script:events.Add("launch:$Serial")
    $script:capturedLaunches.Add(@{ AvdName = $AvdName; GpuRenderer = $GpuRenderer; Headless = [bool]$Headless; DataPath = $DataPath; ReadOnly = [bool]$ReadOnly; SoftwareCpu = [bool]$SoftwareCpu })
    $script:online = $true
    return $true
}

$script:accelerationAvailable = $true
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
    $result = Start-ManagedEmulator -Serial emulator-5554 -AvdName test -SoftwareCpu:$false
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
    if (-not (Start-ManagedEmulator -Serial emulator-5554 -SoftwareCpu:$false @settings)) { throw 'Configured recovery failed' }
    if ($script:capturedLaunches.Count -ne 2) { throw 'Expected primary and fallback launches' }
    foreach ($launch in $script:capturedLaunches) {
        if ($launch.AvdName -cne 'fixture' -or $launch.DataPath -cne $fixture.DataPath -or -not $launch.Headless -or -not $launch.ReadOnly) {
            throw 'Restart or renderer fallback changed the private disk or isolation flags'
        }
    }
    Write-Host "PASS: recovery and fallback preserve private data, headless and read-only settings ($($fixture.DataPath))"
}

# Software CPU mode is explicit and never changes the accelerated default
$script:online = $false
$script:existing = $false
$script:accelerationAvailable = $false
$script:events.Clear()
if (Start-ManagedEmulator -Serial emulator-5554 -AvdName test -SoftwareCpu:$false) {
    throw 'Unavailable acceleration must fail unless software CPU mode is requested'
}
if ($script:events.Count) { throw 'Hardware-only failure must not launch a process' }
$script:ready.Enqueue($true)
if (-not (Start-ManagedEmulator -Serial emulator-5554 -AvdName test -SoftwareCpu -DataPath '/tmp/private.img' -ReadOnly)) {
    throw 'Explicit software CPU mode should work without acceleration'
}
if (-not $script:lastLaunch.SoftwareCpu -or -not $script:lastLaunch.Headless -or $script:lastLaunch.Renderer -ne 'swiftshader') {
    throw 'Software CPU launch did not select headless SwiftShader'
}
$softwareLaunch = $script:capturedLaunches[$script:capturedLaunches.Count - 1]
if ($softwareLaunch.DataPath -cne '/tmp/private.img' -or -not $softwareLaunch.ReadOnly -or -not $softwareLaunch.SoftwareCpu) {
    throw 'Software CPU launch lost private disk or read-only isolation'
}
if (($script:events -join ',') -ne 'launch:emulator-5554,wait:emulator-5554:1200') {
    throw "Software CPU boot budget was not applied: $($script:events -join ',')"
}
$script:events.Clear()
$script:ready.Enqueue($true)
if (-not (Start-ManagedEmulator -Serial emulator-5554 -AvdName test -SoftwareCpu -BootTimeoutSeconds 42)) {
    throw 'Explicit software CPU boot timeout failed'
}
if (($script:events -join ',') -ne 'wait:emulator-5554:42') { throw 'Explicit boot timeout must be preserved' }
Write-Host 'PASS: software CPU opt-in, renderer, boot budget, explicit timeout and accelerated default'

# Exercise the real launcher argument construction without starting an emulator
${function:Start-ManagedEmulatorProcess} = $startProcessImplementation
function Test-RegressionWindowsHost { return $false }
function Get-ManagedEmulatorLaunchLogPaths { return @{ Stdout = 'unused.out'; Stderr = 'unused.err' } }
function Start-Process {
    param($FilePath, $ArgumentList, $RedirectStandardOutput, $RedirectStandardError)
    $script:launchArguments = $ArgumentList
}
if (-not (Start-ManagedEmulatorProcess -AvdName test -Serial emulator-5554 -GpuRenderer swiftshader -Headless -SoftwareCpu)) {
    throw 'Software emulator process launch failed'
}
$arguments = $script:launchArguments -join ' '
foreach ($required in @('-accel off', '-gpu swiftshader', '-no-window', '-cores 2', '-memory 1536')) {
    if (-not $arguments.Contains($required)) { throw "Missing software launch arguments: $required" }
}
if (-not (Start-ManagedEmulatorProcess -AvdName test -Serial emulator-5554)) { throw 'Default launch failed' }
if ($script:launchArguments -contains '-accel') { throw 'Default launch must preserve emulator CPU acceleration selection' }
Write-Host 'PASS: actual software and accelerated emulator command arguments'

# A booted package manager with unavailable external storage is not ready
${function:Wait-EmulatorBootComplete} = $waitBootImplementation
function Adb-Dev-Timeout {
    param($Serial, $AdbArgs, $Seconds)
    if ($AdbArgs -contains 'getprop') { return '1' }
    if ($AdbArgs -contains 'pm') { return 'package:android' }
    if ($AdbArgs[1] -match '^echo ready > /sdcard/\.dxx_ready_') { return $script:storageResult }
    if ($AdbArgs -contains 'settings') { return $script:uiResult }
    throw "Unexpected readiness query: $AdbArgs"
}
$script:uiResult = 'confirmed'
$script:storageResult = $null
if (Wait-EmulatorBootComplete -Serial emulator-5554 -TimeoutSeconds 1) { throw 'Unavailable shared storage was reported ready' }
$script:storageResult = 'ready'
if (-not (Wait-EmulatorBootComplete -Serial emulator-5554 -TimeoutSeconds 1)) { throw 'Mounted writable shared storage was rejected' }
$script:uiResult = $null
if (Wait-EmulatorBootComplete -Serial emulator-5554 -TimeoutSeconds 1) { throw 'Uninitialized fullscreen UI was reported ready' }
$script:uiResult = 'confirmed'
if (-not (Wait-EmulatorBootComplete -Serial emulator-5554 -TimeoutSeconds 1)) { throw 'Ready storage and fullscreen UI were rejected' }
Write-Host 'PASS: boot readiness requires writable shared storage and fullscreen UI initialization'

$script:mountCalls = 0
function Adb-Dev-Timeout {
    param($Serial, $AdbArgs, $Seconds)
    if ($AdbArgs -contains 'list-volumes') { return $script:volumes }
    if ($AdbArgs[1] -ceq "sm mount 'emulated;0'") { $script:mountCalls++; return }
    throw "Unexpected storage repair command: $AdbArgs"
}
foreach ($volumes in @('emulated;0 mounted null', 'emulated;10 unmountable null', '', 'private mounted null')) {
    $script:volumes = $volumes
    Repair-EmulatorSharedStorageMount -Serial emulator-5554
}
if ($script:mountCalls) { throw 'Storage repair touched a healthy, unknown or unrelated volume' }
$script:volumes = "private mounted null`nemulated;0 unmountable null"
Repair-EmulatorSharedStorageMount -Serial emulator-5554
if ($script:mountCalls -ne 1) { throw 'The failed primary emulator volume was not retried' }
try {
    Repair-EmulatorSharedStorageMount -Serial physical-device
    throw 'Storage repair accepted a physical device'
} catch {
    if ($_.Exception.Message -notmatch 'Expected an emulator serial') { throw }
}
Write-Host 'PASS: storage repair is restricted to the failed primary emulator volume'

# The debug APK can be test-only when Gradle selects one emulator ABI
function Adb-Dev-Timeout {
    param($Serial, $AdbArgs, $Seconds, [switch]$IncludeStandardError)
    $script:installArguments = $AdbArgs
    return 'Success'
}
$previousTestApk = $env:DXX_TEST_APK
try {
    $env:DXX_TEST_APK = $PSCommandPath
    if (-not (Install-ApkOnDevice -Serial emulator-5554)) { throw 'Test-only emulator install failed' }
    if ($script:installArguments -notcontains '-t' -or $script:installArguments -notcontains '-d') {
        throw 'Emulator test install must accept test APKs and retain downgrade support'
    }
    if (-not (Install-ApkOnDevice -Serial physical-device)) { throw 'Physical test install failed' }
    if ($script:installArguments -notcontains '-t' -or $script:installArguments -contains '-d') {
        throw 'Physical test install must accept test APKs without enabling downgrades'
    }
} finally {
    $env:DXX_TEST_APK = $previousTestApk
}
Write-Host 'PASS: test-only APK install and physical-device downgrade protection'

function Adb { param($AdbArgs) }
function Wait-SetupCondition {
    param($TimeoutSeconds, $PollMs, [switch]$Lightweight, $Predicate)
    $script:setupTimeout = $TimeoutSeconds
    return $true
}
$previousSoftwareMode = $env:DXX_ANDROID_SOFTWARE_EMULATOR
$previousSerial = $env:ANDROID_SERIAL
try {
    $env:ANDROID_SERIAL = 'emulator-5554'
    $env:DXX_ANDROID_SOFTWARE_EMULATOR = '1'
    if (-not (Wait-SetupActivityReady) -or $script:setupTimeout -ne 180) { throw 'Software launcher readiness budget was not selected' }
    if (-not (Wait-SetupActivityReady -TimeoutSeconds 7) -or $script:setupTimeout -ne 7) { throw 'Explicit launcher readiness timeout changed' }
    $env:DXX_ANDROID_SOFTWARE_EMULATOR = $null
    if (-not (Wait-SetupActivityReady) -or $script:setupTimeout -ne 30) { throw 'Accelerated launcher readiness default changed' }
} finally {
    $env:DXX_ANDROID_SOFTWARE_EMULATOR = $previousSoftwareMode
    $env:ANDROID_SERIAL = $previousSerial
}
Write-Host 'PASS: software launcher startup budget preserves explicit and accelerated defaults'

# Lifecycle budgets are opt-in and cannot change physical target defaults.
$previousSoftwareMode = $env:DXX_ANDROID_SOFTWARE_EMULATOR
$previousSerial = $env:ANDROID_SERIAL
try {
    $env:DXX_ANDROID_SOFTWARE_EMULATOR = '1'
    $env:ANDROID_SERIAL = 'emulator-5554'
    foreach ($case in @(@('Adb', 30), @('Launcher', 180), @('GameStartup', 300), @('Handoff', 180), @('Boot', 1200))) {
        if ((Get-AndroidInfrastructureTimeout -DefaultSeconds 3 -Kind $case[0]) -ne $case[1]) { throw "Wrong budget: $($case[0])" }
    }
    $env:ANDROID_SERIAL = 'physical-device'
    if ((Get-AndroidInfrastructureTimeout -DefaultSeconds 3 -Kind Launcher) -ne 3) { throw 'Physical target budget changed' }
    $env:ANDROID_SERIAL = 'emulator-5554'

    function Start-Sleep { param([int]$Milliseconds) }
    function Adb-Timeout {
        param([string[]]$AdbArgs, [int]$Seconds, [switch]$IncludeStandardError)
        if (-not $IncludeStandardError -or $Seconds -lt 1 -or $Seconds -gt 30) { throw 'Unbounded/ambiguous handoff probe' }
        $script:handoffReads++
        if ($script:handoffSnapshots.Count) { return $script:handoffSnapshots.Dequeue() }
        return '{"result":"LAUNCHER_CONTINUE","next_step":14,"run_id":"ours"}'
    }
    foreach ($terminal in @('cat: files/automation_result.json: No such file or directory',
            '{"result":"PASS","run_id":"ours"}',
            '{"result":"FAIL","run_id":"ours"}',
            '{"result":"LAUNCHER_CONTINUE","next_step":22,"run_id":"ours"}')) {
        $script:handoffReads = 0
        $script:handoffSnapshots = [System.Collections.Queue]::new()
        foreach ($snapshot in @($null, 'malformed', '{"result":"PASS","run_id":"other"}',
                '{"run_id":"ours"}', '{"result":"LAUNCHER_CONTINUE","next_step":2,"run_id":"ours"}')) {
            $script:handoffSnapshots.Enqueue($snapshot)
        }
        for ($i = 0; $i -lt 5; $i++) { $script:handoffSnapshots.Enqueue('{"result":"LAUNCHER_CONTINUE","next_step":14,"run_id":"ours"}') }
        $script:handoffSnapshots.Enqueue($terminal)
        if (-not (Wait-AutomationLauncherHandoff -ExpectedRunId ours -NextStep 14 -TimeoutSeconds 2)) { throw 'Slow valid handoff was not consumed' }
        if ($script:handoffReads -ne 11) { throw 'Unknown/stale handoff snapshot was treated as progress' }
    }
    $script:handoffSnapshots = [System.Collections.Queue]::new()
    if (Wait-AutomationLauncherHandoff -ExpectedRunId ours -NextStep 14 -TimeoutSeconds 1) { throw 'Pending handoff must fail at its deadline' }
    Write-Host 'PASS: bounded handoff waits preserve slow progress and reject unknown/stale snapshots'

    function Confirm-EmulatorHealthWithAdbRecovery { return $true }
    function Test-AppAutomationProcessRunning { return $true }
    function Stop-AppAndWait { throw 'A slow handoff must never force-stop the app' }
    function Wait-AutomationLauncherHandoff {
        param([string]$ExpectedRunId, [int]$NextStep)
        $script:handoffWaits++
        [System.Threading.Thread]::Sleep(1100)
        return $true
    }
    $script:terminalReads = 0
    function Adb-Timeout {
        param([string[]]$AdbArgs, [int]$Seconds, [switch]$IncludeStandardError)
        if ($AdbArgs -contains 'files/automation_result.json') {
            $script:terminalReads++
            if ($script:terminalReads -eq 1) { return '{"result":"LAUNCHER_CONTINUE","next_step":14,"run_id":"ours"}' }
            return '{"result":"PASS","steps_completed":44,"total_steps":44,"run_id":"ours"}'
        }
        return ''
    }
    foreach ($target in @('emulator-5554', 'physical-handoff-fixture')) {
        $env:ANDROID_SERIAL = $target
        $script:terminalReads = 0
        $script:handoffWaits = 0
        if (-not (Watch-AutomationResult -TimeoutSeconds 1 -IsLauncherScript -ExpectedRunId ours)) {
            throw 'A handoff consumed near the overall deadline must allow reading its terminal result'
        }
        if ($script:handoffWaits -ne 1) { throw 'Every target must use the bounded handoff without a force-stop fallback' }
    }
    Write-Host 'PASS: terminal result after a slow handoff wins over the expired outer deadline'

} finally {
    if ($null -ne $previousSoftwareMode) { $env:DXX_ANDROID_SOFTWARE_EMULATOR = $previousSoftwareMode } else { Remove-Item Env:DXX_ANDROID_SOFTWARE_EMULATOR -ErrorAction SilentlyContinue }
    if ($null -ne $previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
