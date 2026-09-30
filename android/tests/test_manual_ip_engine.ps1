param([ValidateSet('d1', 'd2')][string]$Game = 'd2')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$hostSerial = 'emulator-5554'
$clientSerial = 'emulator-5556'
try {
    foreach ($serial in @($hostSerial, $clientSerial)) {
        if (-not (Test-DeviceOnline -Serial $serial)) { throw "$serial is offline" }
        Reset-DeviceGameState -Serial $serial
        if (-not (Start-SetupActivity -Serial $serial)) { throw "Launcher did not start on $serial" }
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-c') -Seconds 5 | Out-Null
    }
    $hostIp = Get-DeviceWlanIp -Serial $hostSerial
    if (-not $hostIp) { throw 'Host Wi-Fi address unavailable' }
    # Direct engine hosting: no launcher lobby or JSON announcement service
    $hostExtras = @('--es', 'game', $Game, '--es', 'mp_mode', 'host', '--es', 'mode', 'coop',
        '--es', 'callsign', 'ProbeHost', '--ei', 'max_players', '2')
    if ($Game -eq 'd2') { $hostExtras += @('--es', 'mission', 'descent') }
    Send-MpCommand -Serial $hostSerial -Command 'lan_launch' -Extras $hostExtras
    if (-not (Wait-ForCondition -Description 'host engine lobby' -TimeoutSec 60 -PollMs 1000 -Condition {
                $intro = Get-GameIntrospection -Serial $hostSerial
                $intro -and $intro.is_network -and $intro.multiplayer.num_connected -eq 1
            })) { throw 'Host did not enter network lobby' }
    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
    $result = Adb-Dev-Timeout -Serial $clientSerial -Seconds 120 -AdbArgs @('shell', 'am', 'instrument', '-w',
        '-e', 'suite', 'engine_query', '-e', 'host', $hostIp, '-e', 'game', $Game,
        'com.dxxredux.app.test/com.dxxredux.app.RecoveryInstrumentation')
    Write-Output $result
    if ($result -notmatch 'PASS: engine query' -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') { throw 'Engine probe checks failed' }
    $intro = Get-GameIntrospection -Serial $hostSerial
    if ($intro.multiplayer.num_connected -ne 1) { throw 'Probe admitted a phantom player' }
    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
    if (-not (Start-SetupActivity -Serial $clientSerial)) { throw 'Client launcher did not restart' }
    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/introspect.json') -Seconds 5 | Out-Null
    Send-MpCommand -Serial $clientSerial -Command 'lan_discover' -Extras @('--es', 'callsign', 'ProbeJoin')
    Send-MpCommand -Serial $clientSerial -Command 'lan_join_ip' -Extras @('--es', 'host_addr', $hostIp)
    if (-not (Wait-ForCondition -Description 'manual IP joins engine-only host' -TimeoutSec 60 -PollMs 1000 -Condition {
                $intro = Get-GameIntrospection -Serial $clientSerial
                $intro -and $intro.in_game -and $intro.is_network -and $intro.multiplayer.num_connected -eq 2
            })) { throw 'Manual IP did not join live engine-only host' }
    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
    if (-not (Wait-ForCondition -Description 'host remains in progress after client disconnects' -TimeoutSec 45 -PollMs 1000 -Condition {
                $intro = Get-GameIntrospection -Serial $hostSerial
                $intro -and $intro.in_game -and $intro.multiplayer.num_connected -eq 1
            })) { throw 'Host did not remain in game after disconnect' }
    if (-not (Start-SetupActivity -Serial $clientSerial)) { throw 'Client launcher did not restart for late join' }
    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/introspect.json') -Seconds 5 | Out-Null
    Send-MpCommand -Serial $clientSerial -Command 'lan_discover' -Extras @('--es', 'callsign', 'ProbeJoin')
    Send-MpCommand -Serial $clientSerial -Command 'lan_join_ip' -Extras @('--es', 'host_addr', $hostIp)
    if (-not (Wait-ForCondition -Description 'manual IP rejoins game in progress' -TimeoutSec 60 -PollMs 1000 -Condition {
                $intro = Get-GameIntrospection -Serial $clientSerial
                $intro -and $intro.in_game -and $intro.is_network -and $intro.multiplayer.num_connected -eq 2
            })) { throw 'Manual IP did not rejoin game in progress' }
    Write-Output "PASS: $Game manual IP joined actual engine host with launcher discovery absent"
} finally {
    foreach ($serial in @($hostSerial, $clientSerial)) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-d') -Seconds 10 |
            Out-File "$PSScriptRoot/../temp/manual_ip_${Game}_$serial.log" -Encoding utf8
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
    }
}
