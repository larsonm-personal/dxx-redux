#!/usr/bin/env pwsh
# Exercise actual Android input-device add/remove callbacks in a running D1 and D2
param([string]$Serial)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$Serial = Initialize-AndroidTestTarget -Serial $Serial
if ($Serial -notlike 'emulator-*') { throw 'This test requires an emulator with the Android uinput shell tool' }
Ensure-EmulatorHealthy | Out-Null
$evidence = Join-Path $script:REPO_ROOT 'android/temp/controller_touch_hotplug'
New-Item -ItemType Directory -Force $evidence | Out-Null
$device = $null

function Assert-TouchState([hashtable]$Expected, [hashtable]$InputStep = @{}, [hashtable]$NativeExpected = @{}) {
    $runId = [guid]::NewGuid().ToString('N')
    $step = @{ action = 'controller_input'; expect_ui = $Expected; post_delay_ms = 100 } + $InputStep
    $steps = @($step)
    if ($NativeExpected.Count) { $steps += @{ action = 'assert'; expect = $NativeExpected } }
    $fixture = Join-Path $evidence 'assert.json'
    ConvertTo-Json -InputObject $steps -Depth 10 | Set-Content -LiteralPath $fixture -Encoding utf8
    Adb -AdbArgs @('push', $fixture, '/data/local/tmp/controller_touch_assert.json') | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/controller_touch_assert.json', 'files/controller_touch_assert.json') | Out-Null
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', 'controller_touch_assert.json', '--es', 'run_id', $runId) | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while ([DateTime]::UtcNow -lt $deadline) {
        $raw = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/automation_result.json')
        if ($raw -and $raw.StartsWith('{')) {
            $result = $raw | ConvertFrom-Json
            if ($result.run_id -eq $runId) {
                if ($result.result -ne 'PASS') { throw "Touch state failed: $raw" }
                return
            }
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out checking touch state $runId"
}

function Connect-TestController {
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $script:ADB
    $start.Arguments = "-s $Serial shell uinput -"
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($start)
    $registration = Get-Content -LiteralPath "$PSScriptRoot/../test_fixtures/controller_touch/gamepad.json" -Raw
    $process.StandardInput.WriteLine($registration)
    $process.StandardInput.Flush()
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($process.HasExited) { throw "uinput failed: $($process.StandardError.ReadToEnd())" }
        $inputs = Adb -AdbArgs @('shell', 'dumpsys', 'input')
        if ($inputs -match 'DXX Touch Coverage Test') {
            Start-Sleep -Milliseconds 300
            return $process
        }
        Start-Sleep -Milliseconds 100
    }
    $process.StandardInput.Close()
    $process.Dispose()
    throw 'Test controller did not register'
}

function Disconnect-TestController([Diagnostics.Process]$Process) {
    $Process.StandardInput.Close()
    if (-not $Process.WaitForExit(5000)) { $Process.Kill() }
    $Process.Dispose()
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ((Adb -AdbArgs @('shell', 'dumpsys', 'input')) -notmatch 'DXX Touch Coverage Test') {
            Start-Sleep -Milliseconds 300
            return
        }
        Start-Sleep -Milliseconds 100
    }
    throw 'Test controller did not disconnect'
}

try {
    foreach ($game in @('d1', 'd2')) {
        Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        # The emulator runner owns its disposable test configuration
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/touch_layout.json', 'files/touch_layout_slots.json') | Out-Null
        Adb -AdbArgs @('logcat', '-c') | Out-Null
        & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_controller_touch_start.jsonc -Game $game -Params @{ GAME = $game } -LeaveRunning -TimeoutSeconds 180 *> (Join-Path $evidence "$game-start.log")
        if ($LASTEXITCODE -ne 0) { throw "Startup failed; see $evidence/$game-start.log" }
        $configBefore = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/controller_config.json')
        $device = Connect-TestController
        $connected = @{
            controller_connected = $true
            touch_sticks = @()
            touch_buttons = @('map', 'btn_2', 'btn_4', 'btn_5')
            touch_selectors = @('Guide', 'PriWpn', 'SecWpn')
        }
        Assert-TouchState $connected
        Assert-TouchState $connected @{ key = 'R2'; pressed = $true } @{ fire_primary_state = @{ gt = 0 } }
        Disconnect-TestController $device
        $device = $null
        Assert-TouchState @{
            controller_connected = $false
            touch_sticks = @('move', 'look', 'stick_2')
            touch_buttons = @('map', 'btn_1', 'btn_2', 'btn_3', 'btn_4', 'btn_5')
        } @{} @{ fire_primary_state = 0 }
        $device = Connect-TestController
        Assert-TouchState $connected
        Disconnect-TestController $device
        $device = $null
        Assert-TouchState @{ controller_connected = $false; touch_sticks = @('move', 'look', 'stick_2') }
        $configAfter = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/controller_config.json')
        if ($configBefore -ne $configAfter) { throw 'Hotplug changed the saved controller configuration' }
        Write-Output "PASS: $game connected, disconnected while firing, reconnected, restored touch, and preserved config"
    }
} finally {
    if ($device) { Disconnect-TestController $device }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
}
