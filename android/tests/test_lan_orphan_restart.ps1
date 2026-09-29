#!/usr/bin/env pwsh
param(
    [string]$Serial = 'emulator-5554',
    [ValidateSet('multiplayer', 'game')][string]$RetryKind = 'multiplayer'
)

$ErrorActionPreference = 'Stop'
if ($Serial -notmatch '^emulator-\d+$') { throw 'Run this lifecycle test only on an emulator' }
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial

function Start-LanJoin {
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.MP_COMMAND', '-p', $script:PACKAGE,
        '--es', 'command', 'lan_launch', '--es', 'mp_mode', 'join', '--es', 'game', 'd2',
        '--es', 'mission', 'descent', '--ei', 'level_num', '2', '--es', 'callsign', 'Restart',
        '--es', 'host_addr', '192.0.2.1') | Out-Null
}

try {
    if (-not (Test-DeviceOnline -Serial $Serial)) { throw 'Emulator is not online' }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Adb -AdbArgs @('shell', 'am', 'start', '-n', "$($script:PACKAGE)/.SetupActivity") | Out-Null
    if (-not (Wait-SetupActivityReady)) { throw 'Launcher did not become ready' }
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '-p', $script:PACKAGE,
        '--es', 'command', 'write_probe_debug_prefs', '--ez', 'enabled', 'true') | Out-Null
    Start-LanJoin
    if (-not (Wait-ForCondition -Description 'native client waiting for unreachable host' -TimeoutSec 40 -Condition {
                (Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-DLOG:D')) -match 'auto_join: waiting for host reply'
            })) { throw 'First join never entered the native engine' }
    $oldPid = (Adb -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game")).Trim()
    if (-not $oldPid) { throw 'First join has no game process' }

    # Returning through the launcher must preserve a healthy, still-returnable engine
    Adb -AdbArgs @('shell', 'am', 'start', '-f', '0x00020000', '-n', "$($script:PACKAGE)/.SetupActivity") | Out-Null
    if (-not (Wait-SetupActivityReady)) { throw 'Launcher did not become ready over the healthy game' }
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '-p', $script:PACKAGE,
        '--es', 'command', 'launch', '--es', 'game', 'd2') | Out-Null
    if (-not (Wait-ForCondition -Description 'return to the healthy game' -TimeoutSec 10 -Condition {
                (Adb -AdbArgs @('shell', 'dumpsys', 'activity', 'activities')) -match '(?:topResumedActivity=|ResumedActivity:).*MainActivity'
            })) { throw 'Play did not return to the healthy game' }
    if ((Adb -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game")).Trim() -ne $oldPid) {
        throw 'Returning to a healthy game replaced its process'
    }

    # CLEAR_TOP finishes MainActivity while its native join loop is still running
    Adb -AdbArgs @('shell', 'am', 'start', '-f', '0x04000000', '-n', "$($script:PACKAGE)/.SetupActivity") | Out-Null
    if (-not (Wait-SetupActivityReady)) { throw 'Launcher did not return' }
    if (-not (Wait-ForCondition -Description 'finished game activity clears its return marker' -TimeoutSec 10 -Condition {
                (Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'ls', 'files/game_activity_state.json')) -match 'No such file'
            })) { throw 'Game activity is still returnable' }
    if ((Adb -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game")).Trim() -ne $oldPid) {
        throw 'Fixture did not leave an orphan native game process'
    }
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    if ($RetryKind -eq 'game') {
        Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '-p', $script:PACKAGE,
            '--es', 'command', 'launch', '--es', 'game', 'd2') | Out-Null
    } else {
        Start-LanJoin
    }
    if (-not (Wait-ForCondition -Description 'second join starts a fresh native engine' -TimeoutSec 40 -Condition {
                $newPid = (Adb -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game")).Trim()
                $log = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-DLOG:D')
                $started = if ($RetryKind -eq 'multiplayer') { $log -match 'auto_join: waiting for host reply' } else {
                    $log -match 'jni startup before-main:'
                }
                $newPid -and $newPid -ne $oldPid -and $started
            })) { throw 'Second join failed to replace the orphan process' }
    if ((Adb -AdbArgs @('logcat', '-d')) -match 'startGame called while native game is already running') {
        throw 'Native duplicate-start guard rejected the second join'
    }
    Write-Host "PASS: abandoned LAN join is replaced by a fresh native engine through $RetryKind"
} finally {
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
