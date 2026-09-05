#!/usr/bin/env pwsh
param([string]$Serial = 'emulator-5554')

$ErrorActionPreference = 'Stop'
if ($Serial -notmatch '^emulator-\d+$') { throw 'Run this fixture test only on an emulator' }
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$root = 'files/d2x-redux'
$save = "$root/Players/save_sets/coop/d2/coopsave.mg5"
$marker = "$root/coop_restore_slot.txt"
$fixtureDir = Join-Path $PSScriptRoot '../temp/coop_compatibility_test'
New-Item -ItemType Directory -Path $fixtureDir -Force | Out-Null

function Send-MpCommand([string]$Command, [string[]]$Extras = @()) {
    Adb -AdbArgs (@('shell', 'am', 'broadcast', '-a', 'com.dxxredux.MP_COMMAND', '-p', $script:PACKAGE,
            '--es', 'command', $Command) + $Extras) | Out-Null
}

function Push-AppFixture([string]$LocalPath, [string]$Destination) {
    Adb -AdbArgs @('push', $LocalPath, '/data/local/tmp/coop-compat-fixture') | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/coop-compat-fixture', $Destination) | Out-Null
}

try {
    if (-not (Test-DeviceOnline -Serial $Serial)) { throw 'Emulator is not online' }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', "$root/Players/save_sets/coop/d2") | Out-Null
    $hadSave = (Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'ls', $save)) -notmatch 'No such file'
    if ($hadSave) {
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', $save, "$save.compat-backup") | Out-Null
    }
    $priorMarker = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', $marker)
    $hadMarker = $priorMarker -notmatch 'No such file'
    if ($hadMarker) {
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', $marker, "$marker.compat-backup") | Out-Null
    }
    Adb -AdbArgs @('shell', 'am', 'start', '-n', "$($script:PACKAGE)/.SetupActivity") | Out-Null
    if (-not (Wait-SetupActivityReady)) { throw 'Launcher did not become ready' }
    Send-MpCommand 'lan_host_lobby' @('--es', 'callsign', 'Compat', '--es', 'game', 'd2', '--es', 'mission', 'd2', '--es', 'mode', 'coop')

    # Missing metadata fixture; native format tests separately cover older versions
    $oldSave = Join-Path $fixtureDir 'old-save.mg5'
    [IO.File]::WriteAllBytes($oldSave, [byte[]](0..63))
    Push-AppFixture $oldSave $save
    $choice = Join-Path $fixtureDir 'choice.txt'
    [IO.File]::WriteAllText($choice, '5')
    Push-AppFixture $choice $marker
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Send-MpCommand 'lan_start_game'
    $blocked = Wait-ForCondition -Description 'incompatible save blocks host start' -TimeoutSec 10 -PollMs 300 -Condition {
        $log = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-NetLog:I')
        # Also inspect service diagnostics, independent of the debug-log toggle
        Send-MpCommand 'lan_lobby_status'
        $log += Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-MP:I')
        $log -match 'This save cannot be used by this build'
    }
    if (-not $blocked) { throw 'Incompatible save did not block hosting with a warning' }
    if (Adb -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game")) { throw 'Blocked host launched a native game' }
    if ((Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'ls', $save)) -notmatch 'coopsave.mg5') {
        throw 'Rejected save was removed'
    }

    [IO.File]::WriteAllText($choice, '{"kind":"fresh"}')
    Push-AppFixture $choice $marker
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Send-MpCommand 'lan_start_game'
    Send-MpCommand 'lan_lobby_status'
    $log = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-MP:I')
    if ($log -notmatch 'at least two players are required') { throw 'Start fresh did not clear the save restriction' }
    Write-Host 'PASS: incompatible save retained, hosting blocked with warning, Start fresh clears restriction'
} finally {
    Send-MpCommand 'lan_stop_lobby'
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', $save, $marker) | Out-Null
    if ($hadSave) {
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mv', "$save.compat-backup", $save) | Out-Null
    }
    if ($hadMarker) {
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mv', "$marker.compat-backup", $marker) | Out-Null
    }
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
