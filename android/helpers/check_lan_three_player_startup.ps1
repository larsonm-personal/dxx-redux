# Shared public fixture for the independent three-player owner and opt-in recovery probes
function Test-LanThreePlayerState($State) {
    if (-not $State -or -not $State.is_network -or $State.multiplayer.num_players -ne 3 -or
        $State.multiplayer.num_connected -ne 3 -or $State.multiplayer.level_num -ne 1 -or
        $State.multiplayer.gamemode_name -cne 'cooperative') { return $false }
    $players = @($State.multiplayer.players)
    if ($players.Count -ne 3 -or @($players | Where-Object { $_.connected -ne 1 }).Count -or
        @($players | Where-Object is_me -EQ $true).Count -ne 1) { return $false }
    $names = @($players | ForEach-Object { ([string]$_.callsign).ToLowerInvariant() } | Sort-Object)
    return ($names -join ',') -ceq 'timingh,timingj,timingk'
}

function Invoke-LanThreePlayerStartupChecks {
    param([ValidateSet('three-player', 'three-player-lost')][string[]]$Cases = @('three-player', 'three-player-lost'))
    $third = if ($env:DXX_THIRD_EMULATOR_SERIAL) { $env:DXX_THIRD_EMULATOR_SERIAL } else { 'emulator-5558' }
    $root = $outputDirectory
    $script:latencyPeerSerials = @($hostSerial, $clientSerial, $third)
    if (@($script:latencyPeerSerials | Sort-Object -Unique).Count -ne 3) { throw 'Three distinct disposable emulators are required' }
    # Verify ALL targets before any app reset, including the recovery experiment path
    Assert-LanPreparationTargets -HostSerial $hostSerial -ClientSerial $clientSerial
    Assert-LanPreparationTargets -HostSerial $hostSerial -ClientSerial $third
    $revision = (git -C $script:REPO_ROOT rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $revision -notmatch '^[0-9a-f]{40}$') { throw 'Exact three-player source revision unavailable' }
    $results = @(foreach ($game in @('d1', 'd2')) {
            foreach ($case in $Cases) {
                [ordered]@{ id = "$game/$case"; status = 'NOT_RUN'; reason = ''; elapsed_seconds = 0 }
            }
        })
    $writeSummary = {
        [ordered]@{ schema = 1; owner = 'test_lan_three_player_startup'; revision = $revision
            category = 'three_player_network'; results = $results
            passed = (@($results | Where-Object { $_.status -ne 'PASS' }).Count -eq 0)
            coverage_complete = (@($results | Where-Object { $_.status -notin @('PASS', 'FAIL') }).Count -eq 0)
        } | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $root 'three-player-summary.json') -Encoding utf8
    }
    & $writeSummary
    try {
        foreach ($game in @('d1', 'd2')) {
            # Copy/hash the same pinned files once per game, not once per scenario
            $gameFiles = @($publicFiles | Where-Object Game -CEQ $game)
            foreach ($serial in $script:latencyPeerSerials) { Stage-PublicLanAssets -Serial $serial -Files $gameFiles }
            foreach ($case in $Cases) {
                $script:latencyGame = $game
                $script:latencySample = $case
                $outputDirectory = Join-Path $root "parallel-startup-checks/$game-$case"
                New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
                $result = @($results | Where-Object { $_.id -ceq "$game/$case" })[0]
                $started = $script:latencyClock.Elapsed.TotalSeconds
                try {
                    foreach ($serial in $script:latencyPeerSerials) {
                        Reset-DeviceGameState -Serial $serial -NoEmulatorRecovery:($serial -eq $third)
                        Assert-PublicLanDeviceAssets -Serial $serial -Files $gameFiles
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
                                    $hostLog -match 'event=wait_requests_flush' -and $hostLog -notmatch 'event=sync_send_begin' -and $clientLog -match 'event=load_hold_begin' -and $clientLog -notmatch 'event=load_hold_end'
                                })) { throw 'Three-player loss window not reached' }
                        $lostPid = ((Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5) -join '').Trim()
                        if ($lostPid -notmatch '^\d+$') { throw 'No live selected player process before loss injection' }
                        Write-LatencyEvent $clientSerial 'three.loss.request' @{ pid = $lostPid }
                        Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                        $stopped = Adb-Dev-Timeout -Serial $clientSerial -AdbArgs @('shell', 'sh', '-c', "'if pidof $($script:PACKAGE):game >/dev/null; then echo RUNNING; else echo STOPPED; fi'") -Seconds 5
                        if ($stopped -cne 'STOPPED') { throw 'Selected player process termination was not verified' }
                        Write-LatencyEvent $clientSerial 'three.loss.stopped'
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
                        Wait-LatencyPeers 'three.briefing.ready' { param($s) (Test-LanThreePlayerState $s) -and $s.coop_briefing.phase -eq 6 -and $s.coop_briefing.presenting } -Timeout 90
                        Invoke-LatencyAutomation 'skip'
                        Wait-LatencyPeers 'three.level.ready' { param($s) (Test-LanThreePlayerState $s) -and $s.in_game -and -not $s.coop_briefing.active -and -not $s.time_paused } -Timeout 90
                        Save-LatencyAutomation 'three'
                    }
                    foreach ($serial in $script:latencyPeerSerials) {
                        $log = (Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 5) -join "`n"
                        if (-not (Test-PublicLanEngineLog -Log $log -Engine $game)) { throw "Expected native $game engine was not observed on $serial" }
                    }
                    Write-LatencyEvent 'all' 'case.pass'
                    $result.status = 'PASS'
                } catch {
                    $result.status = 'FAIL'
                    $result.reason = $_.Exception.Message
                    Write-Output "THREE-PLAYER FAIL $($result.id): $($result.reason)"
                } finally {
                    foreach ($serial in $script:latencyPeerSerials) {
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 15 | Set-Content (Join-Path $outputDirectory "$game-$case-$serial-logcat.txt") -Encoding utf8
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/lan_startup_test.json') -Seconds 5 | Out-Null
                    }
                    $result.elapsed_seconds = [Math]::Round($script:latencyClock.Elapsed.TotalSeconds - $started, 3)
                    $result | ConvertTo-Json | Set-Content (Join-Path $outputDirectory 'case-result.json') -Encoding utf8
                    & $writeSummary
                }
                foreach ($serial in $script:latencyPeerSerials) {
                    if (-not (Test-EmulatorHealthy -Serial $serial)) { throw "Emulator $serial unavailable; remaining cases were not run" }
                }
            }
        }
    } finally {
        & $writeSummary
        $script:latencyPeerSerials = @($hostSerial, $clientSerial)
    }
    if (@($results | Where-Object { $_.status -ne 'PASS' }).Count) { throw 'Three-player startup category did not pass every selected case' }
}
