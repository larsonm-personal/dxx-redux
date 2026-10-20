# Owned by test_lan_launch_preparation; run only after its matched timing samples
function Invoke-LanParallelStartupChecks {
    $timingDirectory = $outputDirectory
    foreach ($game in @('d1', 'd2')) {
        $script:latencyGame = $game
        foreach ($serial in @($hostSerial, $clientSerial)) { Stage-PublicLanAssets -Serial $serial -Files @($publicFiles | Where-Object Game -CEQ $game) }
        foreach ($case in @('slow-host', 'slow-client-duplicate', 'dropped-admission', 'cancel-client', 'retry')) {
            $script:latencySample = $case
            $outputDirectory = Join-Path $timingDirectory "parallel-startup-checks/$game-$case"
            New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
            try {
                foreach ($serial in @($hostSerial, $clientSerial)) {
                    Reset-DeviceGameState -Serial $serial
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/lan_startup_test.json') -Seconds 5 | Out-Null
                    $fault = @{}
                    if ($case -eq 'slow-host' -and $serial -eq $hostSerial) { $fault.delay_ms = 6000 }
                    if ($case -eq 'slow-client-duplicate' -and $serial -eq $clientSerial) { $fault.delay_ms = 6000; $fault.duplicates = 1 }
                    if ($case -eq 'dropped-admission' -and $serial -eq $clientSerial) { $fault.drop_admission = $true }
                    if ($case -eq 'cancel-client' -and $serial -eq $clientSerial) { $fault.delay_ms = 8000 }
                    $faultPath = Join-Path $outputDirectory "$serial-fault.json"
                    $fault | ConvertTo-Json | Set-Content $faultPath -Encoding utf8
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('push', $faultPath, '/data/local/tmp/lan_startup_test.json') -Seconds 10 | Out-Null
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/lan_startup_test.json', 'files/lan_startup_test.json') -Seconds 5 | Out-Null
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-c') -Seconds 5 | Out-Null
                    if (-not (Start-SetupActivity -Serial $serial)) { throw 'Parallel check launcher did not start' }
                }
                $mission = if ($game -eq 'd1') { "''" } else { 'd2demo' }
                Invoke-LatencyCommand $hostSerial 'lan_host_lobby' @('--es', 'callsign', 'TimingH', '--es', 'game', $game, '--es', 'mission', $mission, '--ei', 'max_players', '2')
                Invoke-LatencyCommand $clientSerial 'lan_discover' @('--es', 'callsign', 'TimingJ')
                if (-not (Wait-ForCondition -Description 'Parallel lobby joined' -TimeoutSec 60 -PollMs 250 -Condition {
                            Send-MpCommand -Serial $clientSerial -Command 'lan_join_first_lobby'
                            Test-LanPreparationLobbyPeers -State (Read-LanPreparationLobby $hostSerial) -HostCallsign 'TimingH' -ClientCallsign 'TimingJ'
                        })) { throw 'Parallel lobby join failed' }
                if (-not (Wait-ForCondition -Description 'Parallel lobby ready' -TimeoutSec 30 -PollMs 250 -Condition {
                            Send-MpCommand -Serial $clientSerial -Command 'lan_set_ready' -Extras @('--ez', 'ready', 'true')
                            Test-LanPreparationLobbyPeers -State (Read-LanPreparationLobby $hostSerial) -HostCallsign 'TimingH' -ClientCallsign 'TimingJ' -RequireReady
                        })) { throw 'Parallel lobby readiness failed' }
                Invoke-LatencyCommand $hostSerial 'lan_start_game' @('--ei', 'level_num', '1', '--ez', 'coop_briefings', 'true')
                Invoke-LatencyCommand $hostSerial 'launch_game'
                Invoke-LatencyCommand $clientSerial 'launch_game'
                if ($case -eq 'cancel-client') {
                    if (-not (Wait-ForCondition -Description 'Client admitted before cancellation' -TimeoutSec 30 -PollMs 100 -Condition {
                                $script:cancelClientLog = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                $hostLog = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                $script:cancelClientLog -match 'event=load_hold_begin' -and $hostLog -match 'event=wait_requests_flush'
                            })) { throw 'Cancellation did not reach admitted loading window' }
                    if ($script:cancelClientLog -match 'event=load_hold_end|event=readiness_send') { throw 'Client hold ended before cancellation' }
                    Write-LatencyEvent $clientSerial 'cancel.request'
                    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                    if (-not (Wait-ForCondition -Description 'Host disconnects unloaded client' -TimeoutSec 45 -PollMs 500 -Condition {
                                $state = Get-GameIntrospection -Serial $hostSerial -Fresh
                                Write-LatencyEvent $hostSerial 'cancel.observation' $state
                                $state -and $state.multiplayer.num_connected -eq 1 -and ($state.coop_briefing.phase -eq 6 -or $state.in_game)
                            })) { throw 'Host did not recover after client cancellation' }
                } else {
                    Wait-LatencyPeers 'briefing1.ready' { param($s) $s.coop_briefing.presenting -and $s.coop_briefing.phase -eq 6 }
                    Invoke-LatencyAutomation 'skip'
                    Wait-LatencyPeers 'level1.ready' { param($s) $s.in_game -and -not $s.coop_briefing.active -and -not $s.time_paused -and $s.multiplayer.num_connected -eq 2 }
                    Save-LatencyAutomation 'skip1'
                }
                Write-LatencyEvent 'both' 'case.pass'
            } finally {
                foreach ($serial in @($hostSerial, $clientSerial)) {
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 15 |
                        Set-Content (Join-Path $outputDirectory "$game-$case-$serial-logcat.txt") -Encoding utf8
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/lan_startup_test.json') -Seconds 5 | Out-Null
                }
            }
        }
    }
}
