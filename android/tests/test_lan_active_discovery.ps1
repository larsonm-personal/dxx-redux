#!/usr/bin/env pwsh
param([string]$Serial = 'emulator-5554')

$ErrorActionPreference = 'Stop'
if ($Serial -notmatch '^emulator-\d+$') { throw 'Run this discovery fixture only on an emulator' }
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$prefsPath = 'shared_prefs/dxx_prefs.xml'
$backupCreated = $false
$probe = $null
$replySender = $null
$redirectAdded = $false
$fixtureDir = Join-Path $PSScriptRoot '../temp/lan_active_discovery'
New-Item -ItemType Directory -Path $fixtureDir -Force | Out-Null

try {
    if (-not (Test-DeviceOnline -Serial $Serial)) { throw 'Emulator is not online' }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    $prefsText = Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', $prefsPath)
    [xml]$prefs = $prefsText
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', $prefsPath, "$prefsPath.discovery-backup") | Out-Null
    $backupCreated = $true
    $entry = $prefs.SelectSingleNode('/map/string[@name="recent_lan_ips"]')
    if (-not $entry) {
        $entry = $prefs.CreateElement('string')
        $entry.SetAttribute('name', 'recent_lan_ips')
        $prefs.DocumentElement.AppendChild($entry) | Out-Null
    }
    $entry.InnerText = '10.0.2.2'
    $fixture = Join-Path $fixtureDir 'prefs.xml'
    $prefs.Save($fixture)
    Adb -AdbArgs @('push', $fixture, '/data/local/tmp/lan-query-prefs.xml') | Out-Null
    Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/lan-query-prefs.xml', $prefsPath) | Out-Null
    $probe = [Net.Sockets.UdpClient]::new(42400)
    $probe.Client.ReceiveTimeout = 15000
    $replySender = [Net.Sockets.UdpClient]::new()
    $redirect = Adb -AdbArgs @('emu', 'redir', 'add', 'udp:42490:42400')
    if ($redirect -notmatch 'OK') { throw "Could not forward discovery reply: $redirect" }
    $redirectAdded = $true
    Adb -AdbArgs @('logcat', '-c') | Out-Null
    Adb -AdbArgs @('shell', 'am', 'start', '-n', "$($script:PACKAGE)/.SetupActivity") | Out-Null
    if (-not (Wait-SetupActivityReady)) { throw 'Launcher did not become ready' }
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.MP_COMMAND', '-p', $script:PACKAGE,
        '--es', 'command', 'lan_discover') | Out-Null

    # This host sends no announcements until the client actively queries it
    $sender = [Net.IPEndPoint]::new([Net.IPAddress]::Any, 0)
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    do {
        $packet = $probe.Receive([ref]$sender)
        $request = [Text.Encoding]::UTF8.GetString($packet) | ConvertFrom-Json
    } while ($request.type -ne 'QUERY' -and [DateTime]::UtcNow -lt $deadline)
    if ($request.type -ne 'QUERY') { throw 'Client did not query the remembered host' }
    Write-Output "Received discovery query from ${sender}: $($request | ConvertTo-Json -Compress)"
    $reply = [Text.Encoding]::UTF8.GetBytes('{
        "type":"ANNOUNCE", "protocol_version":2, "query_reply":true,
        "lobby_id":"query-only-fixture", "callsign":"QueryOnlyHost",
        "game":"d2", "mission":"descent", "mode":"coop", "player_count":1, "max_players":4
    }')
    # Use a separate source port for emulator redirection; netsimd owns the query return mapping
    $replySender.Send($reply, $reply.Length, '127.0.0.1', 42490) | Out-Null
    if (-not (Wait-ForCondition -Description 'query-only host appears without manual Find Last Host' -TimeoutSec 10 -Condition {
                Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.MP_COMMAND', '-p', $script:PACKAGE,
                    '--es', 'command', 'lan_discover_status') | Out-Null
                (Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-MP:I')) -match 'lobby: QueryOnlyHost'
            })) { throw 'Client did not discover the query-only host' }
    Adb -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.MP_COMMAND', '-p', $script:PACKAGE,
        '--es', 'command', 'lan_lobby_status') | Out-Null
    if ((Adb -AdbArgs @('logcat', '-d', '-s', 'DXX-MP:I')) -notmatch 'hosting=false joined=false') {
        throw 'Discovery unexpectedly joined the host'
    }
    Write-Host 'PASS: remembered host discovered through active query, without broadcast announcements or auto-join'
} finally {
    # Retain packet handling and discovery status before another test clears logcat
    Adb -AdbArgs @('logcat', '-b', 'all', '-d', '-s', 'DXX-MP:I', 'LobbyService:D', 'AndroidRuntime:E') | Write-Output
    if ($probe) { $probe.Dispose() }
    if ($replySender) { $replySender.Dispose() }
    if ($redirectAdded) { Adb -AdbArgs @('emu', 'redir', 'del', 'udp:42490') | Out-Null }
    Adb -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    if ($backupCreated) {
        Adb -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mv', "$prefsPath.discovery-backup", $prefsPath) | Out-Null
    }
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
