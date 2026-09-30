param([string]$Mission = 'descent')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$hostSerial = 'emulator-5554'
$clientSerial = 'emulator-5556'
function Read-LaunchLog([string]$Serial) {
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @('logcat', '-d', '-s', 'DXX-Launcher:I', 'DXX-MP:I', 'LobbyService:I') -Seconds 5
}
try {
    foreach ($serial in @($hostSerial, $clientSerial)) {
        if (-not (Test-DeviceOnline -Serial $serial)) { throw "$serial is offline" }
        Reset-DeviceGameState -Serial $serial
        if (-not (Start-SetupActivity -Serial $serial)) { throw "Launcher did not start on $serial" }
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-c') -Seconds 5 | Out-Null
    }
    Send-MpCommand -Serial $hostSerial -Command 'lan_host_lobby' -Extras @('--es', 'callsign', 'PrepHost', '--es', 'game', 'd2', '--es', 'mission', $Mission)
    Send-MpCommand -Serial $clientSerial -Command 'lan_discover' -Extras @('--es', 'callsign', 'PrepJoin')
    $joined = Wait-ForCondition -Description 'client joined launcher lobby' -TimeoutSec 25 -PollMs 500 -Condition {
        Send-MpCommand -Serial $clientSerial -Command 'lan_join_first_lobby'
        Send-MpCommand -Serial $hostSerial -Command 'lan_lobby_status'
        (Read-LaunchLog $hostSerial) -match 'players=2'
    }
    if (-not $joined) { throw 'Client did not join launcher lobby' }
    $ready = Wait-ForCondition -Description 'both peers ready' -TimeoutSec 20 -PollMs 500 -Condition {
        Send-MpCommand -Serial $clientSerial -Command 'lan_set_ready' -Extras @('--ez', 'ready', 'true')
        Send-MpCommand -Serial $hostSerial -Command 'lan_lobby_status'
        (Read-LaunchLog $hostSerial) -match 'all_ready=true'
    }
    if (-not $ready) { throw 'Lobby did not become ready' }
    # Do not consume the host launch event yet: this models slow host preparation
    Send-MpCommand -Serial $hostSerial -Command 'lan_start_game'
    $prepared = Wait-ForCondition -Description 'client prepares before host Activity start' -TimeoutSec 30 -PollMs 500 -Condition {
        Send-MpCommand -Serial $clientSerial -Command 'launch_game'
        (Read-LaunchLog $clientSerial) -match 'mp preparation role=client .*step=mission_ready'
    }
    if (-not $prepared) { throw 'Client did not prepare while host launch was pending' }
    if (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'pidof', 'com.dxxredux.app:game') -Seconds 5) {
        throw 'Client entered the engine before host confirmation'
    }
    Send-MpCommand -Serial $hostSerial -Command 'launch_game'
    $launched = Wait-ForCondition -Description 'both prepared peers enter engine' -TimeoutSec 30 -PollMs 500 -Condition {
        $clientLog = Read-LaunchLog $clientSerial
        $hostPid = Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'pidof', 'com.dxxredux.app:game') -Seconds 5
        $clientPid = Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'pidof', 'com.dxxredux.app:game') -Seconds 5
        $hostPid -and $clientPid -and $clientLog -match 'mp preparation role=client .*step=launch_released'
    }
    if (-not $launched) { throw 'Host confirmation did not release prepared client' }
    $log = Read-LaunchLog $clientSerial
    if (([regex]::Matches($log, 'mp preparation role=client .*step=begin')).Count -ne 1) { throw 'Client prepared more than once' }
    Write-Output 'PASS: D1-in-D2 client prepared before host launch, waited for confirmation, then both entered the engine'
} finally {
    foreach ($serial in @($hostSerial, $clientSerial)) {
        Read-LaunchLog $serial | Out-File "$PSScriptRoot/../temp/lobby_launch_$serial.log" -Encoding utf8
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', 'com.dxxredux.app') -Seconds 5 | Out-Null
    }
}
