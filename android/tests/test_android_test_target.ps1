#!/usr/bin/env pwsh
# Target selection, child inheritance and physical-device recovery boundaries
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
. "$PSScriptRoot/../helpers/run_all_tests_profile_menu.ps1"

function Assert-Equal($Expected, $Actual, [string]$Context) {
    if ($Expected -cne $Actual) { throw "$Context expected '$Expected', got '$Actual'" }
}

$priorSerial = $env:ANDROID_SERIAL
$priorPackage = $env:DXX_TEST_PACKAGE
try {
    Assert-Equal 'SingleDevice' (Select-RunAllTestsProfile -ReadChoice { 's' }) 'Single-device suite menu'
    $env:ANDROID_SERIAL = ''
    $env:DXX_TEST_PACKAGE = ''
    Assert-Equal 'emulator-5554' (Initialize-AndroidTestTarget -NonInteractive) 'Unattended default'
    Assert-Equal 'com.dxxredux.app' $script:PACKAGE 'Emulator package'
    function Get-FakeAdbDevices {
        'List of devices attached'
        'retroid device product:RP4Pro model:Retroid_Pocket_4_Pro transport_id:1'
        'unauthorized unauthorized transport_id:2'
        'offline offline transport_id:3'
        'emulator-5554 device model:sdk_gphone64 transport_id:4'
        'emulator-5556 device model:sdk_gphone64 transport_id:5'
        'phone2 device model:Second_Phone transport_id:6'
    }
    $discovered = @(Get-AndroidTestTargets -AdbPath Get-FakeAdbDevices)
    Assert-Equal 'emulator-5554,retroid,phone2' ($discovered.Serial -join ',') 'Discovery excludes unavailable transports and duplicate emulator choices'
    Assert-Equal 'Retroid Pocket 4 Pro [retroid]' $discovered[1].Label 'Device label'
    $targets = @(
        [pscustomobject]@{ Serial = 'emulator-5554'; Label = 'emulator' },
        [pscustomobject]@{ Serial = 'retroid'; Label = 'Retroid Pocket 4 Pro' },
        [pscustomobject]@{ Serial = 'phone2'; Label = 'Second phone' }
    )
    $script:choices = [Collections.Queue]::new()
    foreach ($choice in @('wrong', '0', '4', '2')) { $script:choices.Enqueue($choice) }
    Assert-Equal 'retroid' (Select-AndroidTestTarget -Targets $targets -ReadChoice { $script:choices.Dequeue() }) 'Picker validation'
    Assert-Equal 'emulator-5554' (Select-AndroidTestTarget -Targets $targets -ReadChoice { '' }) 'Picker default'
    Assert-Equal 'phone2' (Initialize-AndroidTestTarget -Serial 'phone2' -NonInteractive) 'Explicit serial overrides environment'
    Assert-Equal 'com.dxxredux.app.nsdtest' $script:PACKAGE 'Isolated physical package'
    Assert-Equal 'phone2' (Initialize-AndroidTestTarget -NonInteractive) 'Inherited selection'
    $child = & (Get-Process -Id $PID).Path -NoProfile -NonInteractive -Command '[Console]::Write("$env:ANDROID_SERIAL|$env:DXX_TEST_PACKAGE")'
    Assert-Equal 'phone2|com.dxxredux.app.nsdtest' $child 'Child target inheritance'
    $broadcast = @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE')
    $scoped = @(Get-TestPackageAdbArguments -Arguments $broadcast)
    Assert-Equal '-p,com.dxxredux.app.nsdtest' ($scoped[-2..-1] -join ',') 'Broadcast package isolation'
    Assert-Equal ($scoped -join ',') ((Get-TestPackageAdbArguments -Arguments $scoped) -join ',') 'Broadcast scope is idempotent'

    $script:powerCalls = @()
    function Adb {
        param($AdbArgs)
        $script:powerCalls += ($AdbArgs -join ' ')
        if ($AdbArgs[1] -eq 'settings') { return '0' }
    }
    function Adb-Dev {
        param($Serial, $AdbArgs)
        Assert-Equal 'phone2' $Serial 'Restore the leased target even if the current target changes'
        $script:powerCalls += ($AdbArgs -join ' ')
        if ($AdbArgs[2] -eq 'get') { return '0' }
    }
    $powerSession = Start-AndroidTestPowerSession
    $env:ANDROID_SERIAL = 'emulator-5554'
    Stop-AndroidTestPowerSession -Session $powerSession
    Assert-Equal $true ($script:powerCalls -contains 'shell svc power stayon true') 'Enable test stay-awake'
    Assert-Equal $true ($script:powerCalls -contains 'shell settings put global stay_on_while_plugged_in 0') 'Restore original power setting'
    $callCount = $script:powerCalls.Count
    Assert-Equal $null (Start-AndroidTestPowerSession) 'Emulator power settings are unchanged'
    Assert-Equal $callCount $script:powerCalls.Count 'No emulator power calls'
    $env:ANDROID_SERIAL = 'phone2'

    function Confirm-EmulatorHealthWithAdbRecovery { return $false }
    $failed = $false
    try { Ensure-EmulatorHealthy | Out-Null } catch { $failed = $_ -match 'unavailable' }
    Assert-Equal $true $failed 'Offline physical target must fail without starting an emulator'
    function Reconnect-AdbDevice { }
    function Confirm-EmulatorHealthWithAdbRecovery { return $true }
    function Wake-AndroidTestTarget { }
    function Start-PrimarySetupActivity { return $true }
    Assert-Equal $true (Invoke-LauncherStartupRecovery) 'Physical recovery restarts only the app'

    # Exercise the real suite preflight with mocked transports and provisioning
    $suitePath = Join-Path $PSScriptRoot '../run_all_tests.ps1'
    $suiteAst = [Management.Automation.Language.Parser]::ParseFile($suitePath, [ref]$null, [ref]$null)
    $preflight = $suiteAst.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-PrimaryEmulatorPreflight'
        }, $true)
    . ([scriptblock]::Create($preflight.Extent.Text))
    $script:singleTestSerial = 'phone2'
    function Ensure-EmulatorHealthy { return $true }
    function Install-AppAndData { param($Serial); Assert-Equal 'phone2' $Serial 'Provision selected phone'; return $true }
    function Invoke-SetupActivityPreflight { param($Serial, [switch]$RequireStandardGameData); return $Serial -eq 'phone2' -and $RequireStandardGameData }
    function Start-SingleEmulator { throw 'Physical preflight must never boot an emulator' }
    Assert-Equal 'phone2' (Invoke-PrimaryEmulatorPreflight -RequireStandardGameData) 'Physical suite preflight'
    function Install-AppAndData { param($Serial); return $false }
    Assert-Equal $null (Invoke-PrimaryEmulatorPreflight -RequireStandardGameData) 'Failed device provisioning must stop preflight'
    Write-Host 'PASS: Android target selection, child inheritance and physical recovery'
} finally {
    $env:ANDROID_SERIAL = $priorSerial
    $env:DXX_TEST_PACKAGE = $priorPackage
}
