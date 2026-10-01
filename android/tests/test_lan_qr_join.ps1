param(
    [ValidateSet('d1', 'd2')][string]$Game = 'd2',
    [ValidateSet('launcher', 'native')][string]$HostLobby = 'native',
    [switch]$CheckLeaveAndJoin,
    [switch]$CheckDisplayedQr
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$hostSerial = 'emulator-5554'
$clientSerial = 'emulator-5556'
function Send-QrCommand {
    param([string]$Serial, [string]$Command, [string[]]$Extras = @())
    Adb-Dev-Timeout -Serial $Serial -AdbArgs (@('shell', 'am', 'broadcast', '-p', $script:PACKAGE,
            '-a', 'com.dxxredux.MP_COMMAND', '--es', 'command', $Command) + $Extras) -Seconds 10 | Out-Null
}
function Read-QrJoinLog([string]$Serial) {
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @('logcat', '-d', '-s', 'DXX-MP:I', 'LobbyService:I', 'DXX-Launcher:I') -Seconds 5
}
function Read-QrLobbyStatus {
    $lines = (Read-QrJoinLog $hostSerial) -split "`n"
    @($lines | Where-Object { $_ -match 'lan_lobby_status: hosting=' }) | Select-Object -Last 1
}
try {
    foreach ($serial in @($hostSerial, $clientSerial)) {
        if (-not (Test-DeviceOnline -Serial $serial)) { throw "$serial is offline" }
        Reset-DeviceGameState -Serial $serial
        if (-not (Start-SetupActivity -Serial $serial)) { throw "Launcher did not start on $serial" }
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-c') -Seconds 5 | Out-Null
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'pm', 'grant', $script:PACKAGE, 'android.permission.NEARBY_WIFI_DEVICES') -Seconds 5 | Out-Null
    }
    $hostIp = Get-DeviceWlanIp -Serial $hostSerial
    if (-not $hostIp) { throw 'Host Wi-Fi address unavailable' }
    $hostExtras = @('--es', 'game', $Game, '--es', 'mode', 'coop', '--es', 'callsign', 'QrHost', '--ei', 'max_players', '2')
    # D1's built-in mission key is empty; D1-in-D2 uses descent
    $hostExtras += @('--es', 'mission', $(if ($Game -eq 'd1') { "''" } else { 'descent' }))
    if ($HostLobby -eq 'native') {
        Send-QrCommand -Serial $hostSerial -Command 'lan_launch' -Extras ($hostExtras + @('--es', 'mp_mode', 'host'))
        if (-not (Wait-ForCondition -Description 'native host lobby' -TimeoutSec 60 -PollMs 1000 -Condition {
                    $intro = Get-GameIntrospection -Serial $hostSerial
                    $intro -and $intro.is_network -and $intro.multiplayer.num_connected -eq 1
                })) { throw 'Host did not enter native lobby' }
    } else {
        Send-QrCommand -Serial $hostSerial -Command 'lan_host_lobby' -Extras $hostExtras
        Send-QrCommand -Serial $hostSerial -Command 'tap_button' -Extras @('--es', 'text', 'Multiplayer')
        if (-not (Wait-ForCondition -Description 'hosting UI visible' -TimeoutSec 15 -PollMs 500 -Condition {
                    (Read-QrJoinLog $hostSerial) -match 'tap_button: tapped'
                })) { throw 'Host lobby UI did not open' }
    }
    Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'screencap', '-p', '/sdcard/lan_qr_host.png') -Seconds 5 | Out-Null
    Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('pull', '/sdcard/lan_qr_host.png',
        "$PSScriptRoot/../temp/lan_qr_${Game}_${HostLobby}_host.png") -Seconds 5 | Out-Null
    if ($CheckDisplayedQr) {
        Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'uiautomator', 'dump', '/sdcard/lan_qr_view.xml') -Seconds 15 | Out-Null
        [xml]$viewTree = Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'cat', '/sdcard/lan_qr_view.xml') -Seconds 5
        $square = $viewTree.SelectNodes('//node') | Where-Object { $_.'content-desc' -match 'Tap to show\s+QR code' } | Select-Object -First 1
        if (-not $square -or $square.bounds -notmatch '^\[(\d+),(\d+)\]\[(\d+),(\d+)\]$') { throw 'Hidden QR square not found' }
        $tapX = [int](($Matches[1] -as [int]) + ($Matches[3] -as [int])) / 2
        $tapY = [int](($Matches[2] -as [int]) + ($Matches[4] -as [int])) / 2
        Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'input', 'tap', "$tapX", "$tapY") -Seconds 5 | Out-Null
        Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'uiautomator', 'dump', '/sdcard/lan_qr_view.xml') -Seconds 15 | Out-Null
        [xml]$viewTree = Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'cat', '/sdcard/lan_qr_view.xml') -Seconds 5
        $revealed = $viewTree.SelectNodes('//node') | Where-Object { $_.'content-desc' -match '^Scan to join ([0-9.]+)\.' } | Select-Object -First 1
        if (-not $revealed -or $revealed.'content-desc' -notmatch '^Scan to join ([0-9.]+)\. Activate') { throw 'QR did not reveal after a tap' }
        $hostIp = $Matches[1]
        Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'screencap', '-p', '/sdcard/lan_qr_revealed.png') -Seconds 5 | Out-Null
        Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('pull', '/sdcard/lan_qr_revealed.png',
            "$PSScriptRoot/../temp/lan_qr_${Game}_${HostLobby}_revealed.png") -Seconds 5 | Out-Null
        Write-Output "Revealed QR invitation: descent://$hostIp"
    }
    # Exercise the exported URI handler and real Compose pending-request path, not a join test hook
    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
    $opened = Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'start', '-W',
        '-a', 'android.intent.action.VIEW', '-d', "descent://$hostIp", $script:PACKAGE) -Seconds 15
    if ($opened -match 'Error:|Exception') { throw "URI routing failed: $opened" }
    if ($HostLobby -eq 'launcher') {
        if (-not (Wait-ForCondition -Description 'URI joined launcher lobby' -TimeoutSec 45 -PollMs 1000 -Condition {
                    Send-QrCommand -Serial $hostSerial -Command 'lan_lobby_status'
                    (Read-QrLobbyStatus) -match 'players=2'
                })) { throw 'URI did not join launcher lobby' }
        if (-not (Wait-ForCondition -Description 'QR client mission ready' -TimeoutSec 30 -PollMs 500 -Condition {
                    if ((Read-QrJoinLog $clientSerial) -notmatch 'tap_button: tapped "Ready"') {
                        Send-QrCommand -Serial $clientSerial -Command 'tap_button' -Extras @('--es', 'text', 'Ready')
                    }
                    Send-QrCommand -Serial $hostSerial -Command 'lan_lobby_status'
                    (Read-QrLobbyStatus) -match 'players=2 all_ready=true'
                })) { throw 'QR client did not become ready' }
        Send-QrCommand -Serial $hostSerial -Command 'lan_start_game'
        Send-QrCommand -Serial $hostSerial -Command 'launch_game'
    }
    if (-not (Wait-ForCondition -Description 'URI client entered game' -TimeoutSec 90 -PollMs 1000 -Condition {
                $intro = Get-GameIntrospection -Serial $clientSerial
                $intro -and $intro.in_game -and $intro.is_network -and $intro.multiplayer.num_connected -eq 2
            })) { throw 'URI did not reach the two-player game' }
    $gamePid = Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5
    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'start', '-W',
        '-a', 'android.intent.action.VIEW', '-d', "descent://$hostIp", $script:PACKAGE) -Seconds 15 | Out-Null
    $retainedPid = Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5
    if (-not $gamePid -or $retainedPid -ne $gamePid) { throw 'Incoming link ended the active game without consent' }
    if (-not (Wait-ForCondition -Description 'active-game invitation cancelled' -TimeoutSec 10 -PollMs 500 -Condition {
                Send-QrCommand -Serial $clientSerial -Command 'tap_button' -Extras @('--es', 'text', 'Cancel')
                (Read-QrJoinLog $clientSerial) -match 'tap_button: tapped "Cancel"'
            })) { throw 'Active-game invitation did not offer Cancel' }
    if ($CheckLeaveAndJoin) {
        Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'start', '-W',
            '-a', 'android.intent.action.VIEW', '-d', "descent://$hostIp", $script:PACKAGE) -Seconds 15 | Out-Null
        if (-not (Wait-ForCondition -Description 'confirmed invitation leaves active game' -TimeoutSec 30 -PollMs 1000 -Condition {
                    if ((Read-QrJoinLog $clientSerial) -notmatch 'tap_button: tapped "Leave"') {
                        Send-QrCommand -Serial $clientSerial -Command 'tap_button' -Extras @('--es', 'text', 'Leave')
                    }
                    $currentPid = Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5
                    $currentPid -ne $gamePid
                })) { throw 'Confirmed invitation did not leave the active game' }
        Write-Output 'PASS: Leave and join ended the old game after confirmation'
    }
    Write-Output "PASS: $Game QR URI joined $HostLobby lobby and reached two-player gameplay"
} finally {
    foreach ($serial in @($hostSerial, $clientSerial)) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-d') -Seconds 10 |
            Out-File "$PSScriptRoot/../temp/lan_qr_${Game}_${HostLobby}_$serial.log" -Encoding utf8
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
    }
}
