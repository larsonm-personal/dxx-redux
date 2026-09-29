#!/usr/bin/env pwsh
param([string]$Serial = 'emulator-5554')

$ErrorActionPreference = 'Stop'
if ($Serial -notmatch '^emulator-\d+$') { throw 'Run this recovery fixture only on an emulator' }
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial

function Send-LanCommand([string]$Command) {
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.MP_COMMAND', '-p', $script:PACKAGE,
        '--es', 'command', $Command) | Out-Null
}

try {
    if (-not (Test-DeviceOnline -Serial $Serial)) { throw 'Emulator is not online' }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('shell', 'am', 'start', '-n', "$($script:PACKAGE)/.SetupActivity") | Out-Null
    if (-not (Wait-SetupActivityReady)) { throw 'Launcher did not become ready' }
    Send-LanCommand 'lan_discover'
    foreach ($retry in @('automatic', 'explicit')) {
        Adb -AdbArgs @('logcat', '-c') | Out-Null
        Send-LanCommand 'lan_fail_recovery'
        if ($retry -eq 'explicit') { Send-LanCommand 'lan_discover' }
        if (-not (Wait-ForCondition -Description "$retry recovery survives a failed reopen" -TimeoutSec 15 -Condition {
                    $log = Adb -AdbArgs @('logcat', '-d', '-s', 'LobbyService:D')
                    $log -match 'Lobby transport recovery failed' -and
                    $log -match 'Injected LAN socket bind failure' -and
                    $log -match 'Socket opened: port=42400'
                })) { throw "$retry recovery did not reopen the socket after a failed bind" }
        Send-LanCommand 'lan_discover_status'
        $status = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-MP:I')
        if ($status -match 'diag=LAN transport unavailable') { throw 'Recovered transport retained a stale error' }
    }
    Send-LanCommand 'lan_stop_lobby'
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Start-Sleep -Seconds 3
    if ((Adb -AdbArgs @('logcat', '-d', '-s', 'LobbyService:D')) -match 'Recovering lobby transport') {
        throw 'Supervisor restarted transport after discovery was stopped'
    }
    Write-Host 'PASS: LAN recovery survives failed rebind, explicit retry works, and Stop retires supervisor'
} finally {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
