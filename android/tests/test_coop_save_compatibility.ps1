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
$probe = $null
$redirectAdded = $false

function Read-HostAnnouncement {
    $query = [Text.Encoding]::UTF8.GetBytes('{"type":"QUERY"}')
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while ([DateTime]::UtcNow -lt $deadline) {
        $probe.Send($query, $query.Length, '127.0.0.1', 42490) | Out-Null
        $sender = [Net.IPEndPoint]::new([Net.IPAddress]::Any, 0)
        try {
            $packet = $probe.Receive([ref]$sender)
            $json = [Text.Encoding]::UTF8.GetString($packet) | ConvertFrom-Json
            if ($json.type -eq 'ANNOUNCE') { return $json }
        } catch [Net.Sockets.SocketException] {
            if ($_.Exception.SocketErrorCode -ne [Net.Sockets.SocketError]::TimedOut) { throw }
        }
    }
    throw 'Host did not reply to the LAN discovery query'
}

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

    # Probe the real discovery reply through the emulator, as a LAN client would
    $probe = [Net.Sockets.UdpClient]::new(42400)
    $probe.Client.ReceiveTimeout = 1000
    $redirect = Adb -AdbArgs @('emu', 'redir', 'add', 'udp:42490:42400')
    if ($redirect -notmatch 'OK') { throw "Could not forward discovery probe: $redirect" }
    $redirectAdded = $true
    $announce = Read-HostAnnouncement
    if ($announce.save_compatibility_warning -notmatch 'This save cannot be used by this build') {
        throw 'Client discovery reply omitted the incompatible-save warning'
    }

    [IO.File]::WriteAllText($choice, '{"kind":"fresh"}')
    Push-AppFixture $choice $marker
    $announce = Read-HostAnnouncement
    if ($announce.PSObject.Properties.Name -contains 'save_compatibility_warning') {
        throw 'Start fresh left the incompatible-save warning visible to clients'
    }
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Send-MpCommand 'lan_start_game'
    Send-MpCommand 'lan_lobby_status'
    $log = Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-MP:I')
    if ($log -notmatch 'at least two players are required') { throw 'Start fresh did not clear the save restriction' }
    Write-Host 'PASS: incompatible save retained, host and client warned, Start fresh clears restriction and client warning'
} finally {
    if ($probe) { $probe.Dispose() }
    if ($redirectAdded) { Adb -AdbArgs @('emu', 'redir', 'del', 'udp:42490') | Out-Null }
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
