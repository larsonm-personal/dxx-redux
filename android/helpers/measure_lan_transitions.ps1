# Opt-in support for test_lan_launch_preparation; two verified disposable emulators
# Controller durations include command/poll overhead. Native script logs are local clocks
$script:latencyPeerSerials = @($hostSerial, $clientSerial)
function Write-LatencyEvent([string]$Peer, [string]$Event, $Data = $null) {
    [ordered]@{ sample = $script:latencySample; game = $script:latencyGame; peer = $Peer
        event = $Event; controller_ms = $script:latencyClock.Elapsed.TotalMilliseconds; data = $Data
    } |
        ConvertTo-Json -Depth 30 -Compress | Add-Content (Join-Path $outputDirectory 'events.jsonl') -Encoding utf8
}

function Wait-LatencyPeers([string]$Event, [scriptblock]$Predicate, [int]$Timeout = 120) {
    $pending = @($script:latencyPeerSerials)
    $deadline = $script:latencyClock.Elapsed.TotalSeconds + $Timeout
    while ($pending.Count -and $script:latencyClock.Elapsed.TotalSeconds -lt $deadline) {
        foreach ($serial in @($pending)) {
            $before = $script:latencyClock.Elapsed.TotalMilliseconds
            $state = Get-GameIntrospection -Serial $serial -Fresh
            Write-LatencyEvent $serial 'observation' @{ request_ms = $before; target = $Event; state = $state }
            if ($state -and (& $Predicate $state)) {
                Write-LatencyEvent $serial $Event @{ request_ms = $before }
                $pending = @($pending | Where-Object { $_ -ne $serial })
            }
        }
    }
    if ($pending.Count) { throw "Timed out observing $Event on $($pending -join ', ')" }
}

function Invoke-LatencyCommand([string]$Serial, [string]$Command, [string[]]$Extras = @()) {
    Write-LatencyEvent $Serial "$Command.request"
    Send-MpCommand -Serial $Serial -Command $Command -Extras $Extras
    Write-LatencyEvent $Serial "$Command.return"
}

function Save-LatencyAutomation([string]$Phase) {
    foreach ($serial in $script:latencyPeerSerials) {
        foreach ($name in @('automation_log.jsonl', 'automation_result.json')) {
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', "files/$name") -Seconds 10 |
                Set-Content (Join-Path $outputDirectory "$($script:latencyGame)-$($script:latencySample)-$serial-$Phase-$name") -Encoding utf8
        }
        $resultPath = Join-Path $outputDirectory "$($script:latencyGame)-$($script:latencySample)-$serial-$Phase-automation_result.json"
        $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json -ErrorAction Stop
        if ($result.result -ne 'PASS') { throw "Timing automation $Phase failed on ${serial}: $($result.reason)" }
    }
}

function Invoke-LatencyAutomation([string]$Phase) {
    $name = $latencyScripts[$Phase]
    $sourcePath = Join-Path $script:REPO_ROOT "android/game_scripts/$name"
    # The native loader accepts JSON, whereas the repository formatter emits
    # JSONC trailing commas. Match the normal automation owner's staging path
    $payload = ConvertFrom-JsoncText -Text ([IO.File]::ReadAllText($sourcePath)) -SourceName $sourcePath
    $null = $payload | ConvertFrom-Json -ErrorAction Stop
    $stagedPath = Join-Path $outputDirectory $name
    [IO.File]::WriteAllText($stagedPath, $payload, [Text.UTF8Encoding]::new($false))
    # Stage on both peers before the timed command; file-copy time is not an engine wait
    foreach ($serial in $script:latencyPeerSerials) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('push', $stagedPath, "/data/local/tmp/$name") -Seconds 30 | Out-Null
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$name", "files/$name") -Seconds 10 | Out-Null
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/automation_result.json', 'files/automation_log.jsonl') -Seconds 5 | Out-Null
    }
    foreach ($serial in $script:latencyPeerSerials) {
        Write-LatencyEvent $serial "$Phase.request"
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', $name) -Seconds 10 | Out-Null
        Write-LatencyEvent $serial "$Phase.return"
    }
}

function Save-LatencyScreen([string]$Phase) {
    foreach ($serial in $script:latencyPeerSerials) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'screencap', '-p', '/data/local/tmp/latency.png') -Seconds 10 | Out-Null
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('pull', '/data/local/tmp/latency.png', (Join-Path $outputDirectory "$($script:latencyGame)-$($script:latencySample)-$serial-$Phase.png")) -Seconds 10 | Out-Null
    }
}

function Invoke-LanTransitionMeasurements {
    $script:latencyClock = [Diagnostics.Stopwatch]::StartNew()
    [ordered]@{ revision = (git -C $script:REPO_ROOT rev-parse HEAD); baseline = '66e5e27da9850a9f142ac5c13d9e65432d48f4e3'
        clock = 'controller Stopwatch; no device clock subtraction'; samples = 3
        polling = 'Get-GameIntrospection -Fresh includes 800 ms sleep per peer plus ADB; observations are upper bounds'
        cache = 'all samples reset configs/preferences and restart processes; sample 1 clears app data; samples 2/3 retain assets/pilots; level 2 is process warm'
        host = $hostSerial; client = $clientSerial
    } | ConvertTo-Json | Set-Content (Join-Path $outputDirectory 'experiment.json')
    foreach ($serial in $script:latencyPeerSerials) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'getprop') -Seconds 10 | Set-Content (Join-Path $outputDirectory "$serial-properties.txt")
    }
    foreach ($game in @('d1', 'd2')) {
        $script:latencyGame = $game
        $files = @($publicFiles | Where-Object Game -CEQ $game)
        foreach ($serial in $script:latencyPeerSerials) { Stage-PublicLanAssets -Serial $serial -Files $files }
        foreach ($sample in 1..3) {
            $script:latencySample = $sample
            try {
                foreach ($serial in $script:latencyPeerSerials) {
                    Reset-DeviceGameState -Serial $serial
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-c') -Seconds 5 | Out-Null
                    Write-LatencyEvent $serial 'launcher.request'
                    if (-not (Start-SetupActivity -Serial $serial)) { throw 'Launcher did not start' }
                    Write-LatencyEvent $serial 'launcher.ready'
                }
                $mission = if ($game -eq 'd1') { "''" } else { 'd2demo' }
                Invoke-LatencyCommand $hostSerial 'lan_host_lobby' @('--es', 'callsign', 'TimingH', '--es', 'game', $game, '--es', 'mission', $mission, '--ei', 'max_players', '2')
                Invoke-LatencyCommand $clientSerial 'lan_discover' @('--es', 'callsign', 'TimingJ')
                if (-not (Wait-ForCondition -Description 'Timing lobby joined' -TimeoutSec 60 -PollMs 250 -Condition {
                            Send-MpCommand -Serial $clientSerial -Command 'lan_join_first_lobby'
                            Test-LanPreparationLobbyPeers -State (Read-LanPreparationLobby $hostSerial) -HostCallsign 'TimingH' -ClientCallsign 'TimingJ'
                        })) { throw 'Timing lobby join failed' }
                Write-LatencyEvent 'both' 'lobby.joined'
                if (-not (Wait-ForCondition -Description 'Timing lobby ready' -TimeoutSec 30 -PollMs 250 -Condition {
                            Send-MpCommand -Serial $clientSerial -Command 'lan_set_ready' -Extras @('--ez', 'ready', 'true')
                            Test-LanPreparationLobbyPeers -State (Read-LanPreparationLobby $hostSerial) -HostCallsign 'TimingH' -ClientCallsign 'TimingJ' -RequireReady
                        })) { throw 'Timing lobby readiness failed' }
                Write-LatencyEvent 'both' 'lobby.ready'
                Invoke-LatencyCommand $hostSerial 'lan_start_game' @('--ei', 'level_num', '1', '--ez', 'coop_briefings', 'true')
                # Release both immediately; no artificial slow-host preparation gate
                Invoke-LatencyCommand $hostSerial 'launch_game'
                Invoke-LatencyCommand $clientSerial 'launch_game'
                Wait-LatencyPeers 'briefing1.ready' { param($s) $s.coop_briefing.presenting -and $s.coop_briefing.phase -eq 6 }
                Save-LatencyScreen 'briefing1'
                Invoke-LatencyAutomation 'skip'
                Wait-LatencyPeers 'level1.ready' { param($s) $s.in_game -and -not $s.coop_briefing.active -and -not $s.time_paused -and $s.multiplayer.num_connected -eq 2 }
                Save-LatencyAutomation 'skip1'
                Save-LatencyScreen 'level1'
                Invoke-LatencyAutomation 'exit'
                Wait-LatencyPeers 'scores.ready' { param($s) $s.postlevel_active }
                Save-LatencyAutomation 'exit'
                Save-LatencyScreen 'scores'
                Invoke-LatencyAutomation 'next'
                Wait-LatencyPeers 'level2.arrived' { param($s) $s.current_level_num -eq 2 }
                Save-LatencyAutomation 'next'
                # D2 shareware may have no authored level-two briefing; record that separately
                $state = Get-GameIntrospection -Serial $hostSerial -Fresh
                if ($state.coop_briefing.active) {
                    Wait-LatencyPeers 'briefing2.ready' { param($s) $s.coop_briefing.presenting -and $s.coop_briefing.phase -eq 6 }
                    Save-LatencyScreen 'briefing2'
                    Invoke-LatencyAutomation 'skip'
                    Wait-LatencyPeers 'level2.ready' { param($s) $s.current_level_num -eq 2 -and $s.in_game -and -not $s.coop_briefing.active -and -not $s.time_paused -and $s.multiplayer.num_connected -eq 2 }
                    Save-LatencyAutomation 'skip2'
                } else {
                    Write-LatencyEvent 'both' 'briefing2.absent'
                    Wait-LatencyPeers 'level2.ready' { param($s) $s.current_level_num -eq 2 -and $s.in_game -and -not $s.time_paused -and $s.multiplayer.num_connected -eq 2 }
                }
                Save-LatencyScreen 'level2'
                Write-LatencyEvent 'both' 'sample.pass'
            } finally {
                foreach ($serial in $script:latencyPeerSerials) {
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-d', '-v', 'monotonic') -Seconds 15 |
                        Set-Content (Join-Path $outputDirectory "$game-$sample-$serial-logcat.txt") -Encoding utf8
                    Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
                }
            }
        }
    }
}
