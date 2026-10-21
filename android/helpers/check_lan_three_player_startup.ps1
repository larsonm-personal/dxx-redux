# Public disposable-emulator coverage owned by test_lan_launch_preparation
function Invoke-LanThreePlayerStartupChecks {
    param([string[]]$Cases = @('three-player', 'three-player-lost'))
    $third = 'emulator-5558'
    Assert-LanPreparationTargets -HostSerial $hostSerial -ClientSerial $third
    $root = $outputDirectory
    $script:latencyPeerSerials = @($hostSerial, $clientSerial, $third)
    try {
        foreach ($game in @('d1', 'd2')) {
            foreach ($case in $Cases) {
                $script:latencyGame = $game
                $script:latencySample = $case
                $outputDirectory = Join-Path $root "parallel-startup-checks/$game-$case"
                New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
                try {
                    foreach ($serial in $script:latencyPeerSerials) {
                        Stage-PublicLanAssets -Serial $serial -Files @($publicFiles | Where-Object Game -CEQ $game)
                        Reset-DeviceGameState -Serial $serial
                        $delay = if ($serial -eq $clientSerial) { 20000 } elseif ($serial -eq $third) { 6000 } else { 0 }
                        $path = Join-Path $outputDirectory "$serial-fault.json"
                        @{ delay_ms = $delay } | ConvertTo-Json | Set-Content $path -Encoding utf8
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @('push', $path, '/data/local/tmp/lan_startup_test.json') -Seconds 10 | Out-Null
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/lan_startup_test.json', 'files/lan_startup_test.json') -Seconds 5 | Out-Null
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-c') -Seconds 5 | Out-Null
                        if (-not (Start-SetupActivity -Serial $serial)) { throw 'Three-player launcher failed' }
                    }
                    $mission = if ($game -eq 'd1') { "''" } else { 'd2demo' }
                    Invoke-LatencyCommand $hostSerial 'lan_host_lobby' @('--es', 'callsign', 'TimingH', '--es', 'game', $game, '--es', 'mission', $mission, '--ei', 'max_players', '3')
                    foreach ($serial in @($clientSerial, $third)) {
                        $callsign = if ($serial -eq $third) { 'TimingK' } else { 'TimingJ' }
                        Invoke-LatencyCommand $serial 'lan_discover' @('--es', 'callsign', $callsign)
                        if (-not (Wait-ForCondition -Description "Join third-player lobby $serial" -TimeoutSec 45 -PollMs 500 -Condition {
                                    Send-MpCommand -Serial $serial -Command 'lan_join_first_lobby'
                                    $state = Read-LanPreparationLobby $hostSerial
                                    $state -and @($state.lan.players | Where-Object callsign -CEQ $callsign).Count -eq 1
                                })) { throw 'Three-player lobby join failed' }
                        Send-MpCommand -Serial $serial -Command 'lan_set_ready' -Extras @('--ez', 'ready', 'true')
                    }
                    if (-not (Wait-ForCondition -Description 'All three launcher players ready' -TimeoutSec 30 -PollMs 500 -Condition {
                                $state = Read-LanPreparationLobby $hostSerial
                                $state -and @($state.lan.players).Count -eq 3 -and @($state.lan.players | Where-Object { $_.connected -and $_.ready }).Count -eq 3
                            })) { throw 'Three-player ready roster incomplete' }
                    Invoke-LatencyCommand $hostSerial 'lan_start_game' @('--ei', 'level_num', '1', '--ez', 'coop_briefings', 'true')
                    foreach ($serial in $script:latencyPeerSerials) { Invoke-LatencyCommand $serial 'launch_game' }
                    if ($case -eq 'three-player-lost') {
                        if (-not (Wait-ForCondition -Description 'Three-player admitted loading window' -TimeoutSec 35 -PollMs 100 -Condition {
                                    $hostLog = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                    $clientLog = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                    $hostLog -match 'event=wait_requests_flush' -and $clientLog -match 'event=load_hold_begin' -and $clientLog -notmatch 'event=load_hold_end'
                                })) { throw 'Three-player loss window not reached' }
                        Write-LatencyEvent $clientSerial 'three.loss.request'
                        Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                        if (-not (Wait-ForCondition -Description 'Both surviving peers recover without starting' -TimeoutSec 90 -PollMs 1000 -Condition {
                                    $hostLog = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                    $thirdLog = (Adb-Dev-Timeout -Serial $third -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                    if ($hostLog -match 'event=sync_send_begin' -or $thirdLog -match 'event=sync_received') { throw 'Survivors silently started without selected player' }
                                    $hostLog -match 'event=initial_load_peer_lost' -and $thirdLog -match 'event=initial_sync_timeout'
                                })) { throw 'Three-player survivors stranded after loss' }
                        foreach ($serial in @($hostSerial, $third)) {
                            $message = if ($serial -eq $hostSerial) { 'A selected player did not finish loading within 60 seconds' } else { 'The host did not finish starting within 60 seconds' }
                            Assert-LanRecoveryMenu $serial 'three.loss.terminal' $message
                        }
                    } else {
                        Wait-LatencyPeers 'three.briefing.ready' { param($s) $s.coop_briefing.phase -eq 6 -and $s.coop_briefing.presenting -and $s.multiplayer.num_connected -eq 3 }
                        Invoke-LatencyAutomation 'skip'
                        Wait-LatencyPeers 'three.level.ready' { param($s) $s.in_game -and -not $s.coop_briefing.active -and -not $s.time_paused -and $s.multiplayer.num_connected -eq 3 }
                        Save-LatencyAutomation 'three'
                    }
                    Write-LatencyEvent 'all' 'case.pass'
                } finally {
                    foreach ($serial in $script:latencyPeerSerials) {
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 15 | Set-Content (Join-Path $outputDirectory "$game-$case-$serial-logcat.txt") -Encoding utf8
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/lan_startup_test.json') -Seconds 5 | Out-Null
                    }
                }
            }
        }
    } finally { $script:latencyPeerSerials = @($hostSerial, $clientSerial) }
}
