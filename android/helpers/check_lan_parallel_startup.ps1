# Owned by test_lan_launch_preparation; run only after its matched timing samples
function Test-LanRecoveryMenu($State, [string]$ErrorText = '') {
    if (-not $State -or $State.in_game -or $State.game_window_is_front -or $State.screen_mode -ne 'menu' -or -not $State.menu -or $State.menu.type -ne 'newmenu') { return $false }
    # The ordinary synchronization Wait dialog is also a non-game menu
    if ($State.menu.subtitle -match '^\s*Wait') { return $false }
    $items = @($State.menu.items | ForEach-Object { ([string]$_.text).Trim() })
    $text = (@($State.menu.title, $State.menu.subtitle) + $items) -join "`n"
    if ($ErrorText -and $text -match [regex]::Escape($ErrorText)) { return $true }
    return -not $State.is_network -and $items -contains 'New game' -and $items -contains 'Options' -and $items -contains 'Quit'
}

function Assert-LanRecoveryMenu([string]$Serial, [string]$Event, [string]$ErrorText = '') {
    $deadline = $script:latencyClock.Elapsed.TotalSeconds + 10
    do {
        $state = Get-GameIntrospection -Serial $Serial -Fresh
        if (Test-LanRecoveryMenu $state $ErrorText) {
            Write-LatencyEvent $Serial $Event $state
            return
        }
        Start-Sleep -Milliseconds 200
    } while ($script:latencyClock.Elapsed.TotalSeconds -lt $deadline)
    Write-LatencyEvent $Serial "$Event.failed" $state
    throw "No main menu or expected recovery dialog on $Serial"
}

function Invoke-LanParallelStartupChecks {
    param([string[]]$Games = @('d1', 'd2'),
        [string[]]$Cases = @('slow-host', 'slow-client-duplicate', 'long-client', 'dropped-admission', 'cancel-client', 'retry'))
    $timingDirectory = $outputDirectory
    foreach ($game in $Games) {
        $script:latencyGame = $game
        foreach ($serial in @($hostSerial, $clientSerial)) { Stage-PublicLanAssets -Serial $serial -Files @($publicFiles | Where-Object Game -CEQ $game) }
        foreach ($case in $Cases) {
            $script:latencySample = $case
            $outputDirectory = Join-Path $timingDirectory "parallel-startup-checks/$game-$case"
            New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
            try {
                foreach ($serial in @($hostSerial, $clientSerial)) {
                    Reset-DeviceGameState -Serial $serial
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/lan_startup_test.json') -Seconds 5 | Out-Null
                    $fault = @{}
                    if ($case -in @('host-loss-loading', 'cancel-reconnect') -and $serial -eq $hostSerial) { $fault.delay_ms = 20000 }
                    if ($case -eq 'host-loss-waiting' -and $serial -eq $clientSerial) { $fault.delay_ms = 20000 }
                    if ($case -eq 'slow-host' -and $serial -eq $hostSerial) { $fault.delay_ms = 6000 }
                    if ($case -eq 'slow-client-duplicate' -and $serial -eq $clientSerial) { $fault.delay_ms = 6000; $fault.duplicates = 1 }
                    if ($case -eq 'long-client' -and $serial -eq $clientSerial) { $fault.delay_ms = 20000 }
                    if ($case -eq 'dropped-admission' -and $serial -eq $clientSerial) { $fault.drop_admission = $true }
                    if ($case -in @('cancel-client', 'same-session-reconnect') -and $serial -eq $clientSerial) { $fault.delay_ms = 8000 }
                    $packetCases = @('replay-admission', 'retag-admission', 'drop-requests', 'drop-proof', 'delay-proof', 'replay-proof', 'delay-admission', 'failed-level-load')
                    if ($case -in $packetCases -and $serial -eq $clientSerial) { $fault.packet_fault = [Array]::IndexOf($packetCases, $case) + 1 }
                    $faultPath = Join-Path $outputDirectory "$serial-fault.json"
                    $fault | ConvertTo-Json | Set-Content $faultPath -Encoding utf8
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('push', $faultPath, '/data/local/tmp/lan_startup_test.json') -Seconds 10 | Out-Null
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/lan_startup_test.json', 'files/lan_startup_test.json') -Seconds 5 | Out-Null
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-c') -Seconds 5 | Out-Null
                    if (-not (Start-SetupActivity -Serial $serial)) { throw 'Parallel check launcher did not start' }
                }
                $mission = if ($game -eq 'd1') { "''" } else { 'd2demo' }
                $maxPlayers = if ($case -eq 'missing-third') { '3' } else { '2' }
                Invoke-LatencyCommand $hostSerial 'lan_host_lobby' @('--es', 'callsign', 'TimingH', '--es', 'game', $game, '--es', 'mission', $mission, '--ei', 'max_players', $maxPlayers)
                Invoke-LatencyCommand $clientSerial 'lan_discover' @('--es', 'callsign', 'TimingJ')
                if (-not (Wait-ForCondition -Description 'Parallel lobby joined' -TimeoutSec 60 -PollMs 250 -Condition {
                            Send-MpCommand -Serial $clientSerial -Command 'lan_join_first_lobby'
                            Test-LanPreparationLobbyPeers -State (Read-LanPreparationLobby $hostSerial) -HostCallsign 'TimingH' -ClientCallsign 'TimingJ'
                        })) { throw 'Parallel lobby join failed' }
                if (-not (Wait-ForCondition -Description 'Parallel lobby ready' -TimeoutSec 30 -PollMs 250 -Condition {
                            Send-MpCommand -Serial $clientSerial -Command 'lan_set_ready' -Extras @('--ez', 'ready', 'true')
                            Test-LanPreparationLobbyPeers -State (Read-LanPreparationLobby $hostSerial) -HostCallsign 'TimingH' -ClientCallsign 'TimingJ' -RequireReady
                        })) { throw 'Parallel lobby readiness failed' }
                $hostPidBefore = $null
                Invoke-LatencyCommand $hostSerial 'lan_start_game' @('--ei', 'level_num', '1', '--ez', 'coop_briefings', 'true')
                Invoke-LatencyCommand $hostSerial 'launch_game'
                Invoke-LatencyCommand $clientSerial 'launch_game'
                if ($case -eq 'failed-level-load') {
                    if (-not (Wait-ForCondition -Description 'Real missing-level load failure' -TimeoutSec 40 -PollMs 500 -Condition {
                                $log = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                $log -match "Error:.*__missing_startup_test_level__"
                            })) { throw 'Expected level-loader failure not observed' }
                    $log = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                    if ($log -match 'event=readiness_send') { throw 'Failed loader asserted readiness' }
                    if (-not (Wait-ForCondition -Description 'Host reports failed initial load' -TimeoutSec 80 -PollMs 1000 -Condition {
                                $log = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                if ($log -match 'event=sync_send_begin') { throw 'Host started with a failed loader' }
                                $log -match 'event=initial_load_peer_lost'
                            })) { throw 'Host did not report failed selected loader' }
                } elseif ($case -eq 'missing-third') {
                    if (-not (Wait-ForCondition -Description 'Early reservation before missing third player' -TimeoutSec 30 -PollMs 500 -Condition {
                                $log = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                $log -match 'event=admission_received'
                            })) { throw 'No early reservation observed' }
                    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                    if (-not (Wait-ForCondition -Description 'Host abandons incomplete initial roster' -TimeoutSec 75 -PollMs 1000 -Condition {
                                $log = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                if ($log -match 'event=sync_send_begin') { throw 'Incomplete roster synchronized' }
                                $log -match 'event=initial_selection_timeout'
                            })) { throw 'Dead reservation stranded host selection' }
                } elseif ($case -like 'host-loss-*') {
                    $hostMarker = if ($case -eq 'host-loss-loading') { 'event=load_hold_begin' } else { 'event=wait_requests_flush' }
                    if (-not (Wait-ForCondition -Description 'Host loss injection boundary' -TimeoutSec 40 -PollMs 100 -Condition {
                                $hostLog = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                $clientLog = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                $hostLog -match $hostMarker -and $clientLog -match 'event=admission_send'
                            })) { throw 'Host loss injection boundary not reached' }
                    $hostLog = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                    if ($hostLog -match 'event=sync_send_begin') { throw 'Host already synchronized before loss injection' }
                    Write-LatencyEvent $hostSerial 'host-loss.request'
                    Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                    if (-not (Wait-ForCondition -Description 'Client leaves initial sync after silent host loss' -TimeoutSec 85 -PollMs 1000 -Condition {
                                $clientLog = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                $clientLog -match 'event=initial_sync_timeout' -and $clientLog -match 'event=sync_wait_aborted' -and $clientLog -match 'event=start_level_end'
                            })) { throw 'Client stranded in initial synchronization after silent host loss (85s bound)' }
                    $clientLog = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                    if ($clientLog -match 'event=sync_received') { throw 'Unexpected sync after host loss' }
                    Assert-LanRecoveryMenu $clientSerial 'host-loss.recovered' 'The host did not finish starting within 60 seconds'
                } elseif ($case -in @('cancel-client', 'same-session-reconnect', 'cancel-reconnect')) {
                    if (-not (Wait-ForCondition -Description 'Client admitted before cancellation' -TimeoutSec 30 -PollMs 100 -Condition {
                                $script:cancelClientLog = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                $hostLog = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                if ($case -eq 'cancel-reconnect') { $script:cancelClientLog -match 'event=readiness_send' -and $hostLog -match 'event=load_hold_begin' -and $hostLog -notmatch 'event=load_hold_end' }
                                else { $script:cancelClientLog -match 'event=load_hold_begin' -and $hostLog -match 'event=wait_requests_flush' }
                            })) { throw 'Cancellation did not reach admitted loading window' }
                    if ($case -eq 'cancel-reconnect') {
                        $latencyScripts.cancel = 'test_lan_startup_cancel.jsonc'
                        $script:latencyPeerSerials = @($clientSerial)
                        try {
                            Invoke-LatencyAutomation 'cancel'
                            if (-not (Wait-ForCondition -Description 'Native Escape cancels join' -TimeoutSec 10 -PollMs 250 -Condition {
                                        $log = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                        $log -match 'event=sync_wait_aborted' -and $log -match 'event=start_level_end'
                                    })) { throw 'Native join cancellation not observed' }
                            Save-LatencyAutomation 'cancel'
                            Assert-LanRecoveryMenu $clientSerial 'cancel.terminal'
                        } finally { $script:latencyPeerSerials = @($hostSerial, $clientSerial) }
                    } elseif ($script:cancelClientLog -match 'event=load_hold_end|event=readiness_send') { throw 'Client hold ended before cancellation' }
                    $hostPidBefore = ((Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5) -join '').Trim()
                    if ($hostPidBefore -notmatch '^\d+$') { throw 'No definite surviving host process' }
                    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 10 | Set-Content (Join-Path $outputDirectory 'client-before-reconnect-logcat.txt') -Encoding utf8
                    Write-LatencyEvent $hostSerial 'reconnect.host-process-before' @{ pid = $hostPidBefore }
                    Write-LatencyEvent $clientSerial 'cancel.request'
                    Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                    if ($case -in @('same-session-reconnect', 'cancel-reconnect')) {
                        Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/lan_startup_test.json') -Seconds 5 | Out-Null
                        if (-not (Start-SetupActivity -Serial $clientSerial)) { throw 'Reconnect launcher did not start' }
                        Invoke-LatencyCommand $clientSerial 'lan_discover' @('--es', 'callsign', 'TimingJ')
                        if (-not (Wait-ForCondition -Description 'Same-session native reconnect begins' -TimeoutSec 25 -PollMs 500 -Condition {
                                    Send-MpCommand -Serial $clientSerial -Command 'lan_join_first_lobby'
                                    $log = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                    ([regex]::Matches($log, 'event=join_mission_begin')).Count -ge 1
                                })) { throw 'Same-session reconnect did not reach native join' }
                        Wait-LatencyPeers 'reconnect.briefing.ready' { param($s) $s.coop_briefing.presenting -and $s.coop_briefing.phase -eq 6 -and $s.multiplayer.num_connected -eq 2 } 90
                        Invoke-LatencyAutomation 'skip'
                        Wait-LatencyPeers 'reconnect.level.ready' { param($s) $s.in_game -and -not $s.coop_briefing.active -and $s.multiplayer.num_connected -eq 2 }
                        Save-LatencyAutomation 'reconnect'
                        $hostLog = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                        $hostPidAfter = ((Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5) -join '').Trim()
                        Write-LatencyEvent $hostSerial 'reconnect.host-process-after' @{ pid = $hostPidAfter }
                        if ($hostPidAfter -cne $hostPidBefore) { throw 'Host process changed during reconnect' }
                        if (([regex]::Matches($hostLog, 'event=start_level_begin')).Count -ne 1) { throw 'Host restarted during same-session test' }
                    } elseif (-not (Wait-ForCondition -Description 'Host disconnects unloaded client' -TimeoutSec 90 -PollMs 500 -Condition {
                                $state = Get-GameIntrospection -Serial $hostSerial -Fresh
                                Write-LatencyEvent $hostSerial 'cancel.observation' $state
                                $hostLog = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                if ($hostLog -match 'event=sync_send_begin') { throw 'Host silently started after losing a selected player' }
                                $hostLog -match 'event=initial_load_peer_lost'
                            })) { throw 'Host did not recover after client cancellation' }
                } else {
                    Wait-LatencyPeers 'briefing1.ready' { param($s) $s.coop_briefing.presenting -and $s.coop_briefing.phase -eq 6 }
                    if ($case -in @('replay-admission', 'retag-admission', 'delay-proof', 'replay-proof', 'delay-admission')) {
                        if (-not (Wait-ForCondition -Description 'Delayed datagram released' -TimeoutSec 15 -PollMs 500 -Condition {
                                    $log = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                    $log -match '\[STARTUP-FAULT\] release'
                                })) { throw 'Configured delayed wire fault was not exercised' }
                    }
                    if ($case -eq 'retag-admission') {
                        $log = (Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                        if ($log -notmatch '\[STARTUP-AUTH\] signature_rejected' -or $log -match 'event=admission_received') { throw 'Retag rejection was not discriminated from stale-counter rejection' }
                    }
                    if ($case -in @('drop-requests', 'drop-proof')) {
                        $log = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                        $expectedDrops = if ($case -eq 'drop-requests') { 2 } else { 1 }
                        if (([regex]::Matches($log, '\[STARTUP-FAULT\] drop')).Count -ne $expectedDrops) { throw 'Configured packet loss was not exercised' }
                    }
                    Invoke-LatencyAutomation 'skip'
                    Wait-LatencyPeers 'level1.ready' { param($s) $s.in_game -and -not $s.coop_briefing.active -and -not $s.time_paused -and $s.multiplayer.num_connected -eq 2 }
                    Save-LatencyAutomation 'skip1'
                    if ($case -in @('old-host', 'old-client')) {
                        $hostPidBefore = ((Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5) -join '').Trim()
                        if ($hostPidBefore -notmatch '^\d+$') { throw 'No definite mixed-version host process' }
                        Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 10 | Set-Content (Join-Path $outputDirectory 'client-before-reconnect-logcat.txt') -Encoding utf8
                        if (-not (Start-SetupActivity -Serial $clientSerial)) { throw 'Mixed-version reconnect launcher failed' }
                        Invoke-LatencyCommand $clientSerial 'lan_discover' @('--es', 'callsign', 'TimingJ')
                        if (-not (Wait-ForCondition -Description 'Mixed-version same-session native rejoin' -TimeoutSec 30 -PollMs 500 -Condition {
                                    Send-MpCommand -Serial $clientSerial -Command 'lan_join_first_lobby'
                                    $log = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                                    $log -match 'auto_net: starting auto-join'
                                })) { throw 'Mixed-version reconnect did not begin' }
                        Wait-LatencyPeers 'mixed.reconnected' { param($s) $s.in_game -and -not $s.coop_briefing.active -and -not $s.time_paused -and $s.multiplayer.num_connected -eq 2 }
                        $hostPidAfter = ((Adb-Dev-Timeout -Serial $hostSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5) -join '').Trim()
                        if ($hostPidAfter -cne $hostPidBefore) { throw 'Mixed-version host restarted' }
                        Write-LatencyEvent $hostSerial 'mixed.same-host' @{ before = $hostPidBefore; after = $hostPidAfter }
                        if ($case -eq 'old-host') {
                            $log = (Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                            if ($log -notmatch 'event=join_query_legacy_fallback') { throw 'Legacy query fallback not exercised by actual old host' }
                        }
                    }
                }
                if ($case -in @('cancel-client', 'missing-third', 'failed-level-load')) {
                    $message = if ($case -eq 'missing-third') { 'Not all selected players connected within 60 seconds' } else { 'A selected player did not finish loading within 60 seconds' }
                    Assert-LanRecoveryMenu $hostSerial 'host-failure.terminal' $message
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
