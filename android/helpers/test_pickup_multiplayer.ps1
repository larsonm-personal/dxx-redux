# Three-player coverage owned by test_lan; uses its correlated automation helpers
function Invoke-CoopPickupRelayScenario {
    $serials = @($EMU1, $EMU2, $PickupRelayDevice)
    if (@($serials | Select-Object -Unique).Count -ne 3) { throw 'Pickup relay requires three distinct devices' }
    $avds = @{ $EMU1 = $HostAvd; $EMU2 = $JoinAvd; $PickupRelayDevice = $PickupRelayAvd }
    $captures = @()
    $states = @{}
    $output = Join-Path $REPO_ROOT "temp/coop-pickup-relay-$Game-$PickupRelayCase"
    & "$REPO_ROOT/android/helpers/retain-recent-artifacts.ps1" -Artifacts $output | Out-Null
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    try {
        foreach ($serial in $serials) {
            Start-EmulatorIfNeeded -Serial $serial -AvdMap $avds
            if (-not (Ensure-StandardGameDataOnDevice -Serial $serial)) { throw "Missing game data on $serial" }
            Reset-DeviceGameState -Serial $serial
            if (-not (Start-SetupActivity -Serial $serial -TimeoutSec $TimeoutSeconds)) { throw "Launcher unavailable on $serial" }
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
                '--es', 'command', 'write_music_prefs', '--es', 'source', 'midi',
                '--ez', 'prefer_mission_soundtrack', 'false', '--ei', 'play_order', '0', '--ei', 'volume', '8') -Seconds 10 | Out-Null
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-c') -Seconds 5 | Out-Null
            $captures += Start-LanLogcatCapture -Serial $serial -OutputPath "$output/$serial.log" -ErrorPath "$output/$serial.err.log"
        }
        $hostIp = Get-DeviceWlanIp -Serial $EMU1
        if (-not $hostIp) { throw 'Host has no Wi-Fi address' }
        foreach ($serial in $serials[1..2]) {
            if (-not (Wait-ForCondition -Description "$serial can reach relay host" -TimeoutSec 60 -Condition {
                        $ping = Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'ping', '-c', '1', '-W', '1', $hostIp) -Seconds 5
                        return $ping -match '1 received'
                    })) { throw "No direct LAN route from $serial to $hostIp" }
        }
        $common = @('--es', 'game', $Game, '--es', 'mode', 'coop', '--ei', 'max_players', '3',
            '--ei', 'level_num', "$InitialLevel", '--ei', 'difficulty', '1')
        if ($MISSION) { $common += @('--es', 'mission', $MISSION) }
        Send-MpCommand -Serial $EMU1 -Command 'lan_launch' -Extras ($common + @('--es', 'mp_mode', 'host', '--es', 'callsign', $CALLSIGN1))
        if (-not (Wait-ForCondition -Description 'Three-player host lobby' -TimeoutSec $TimeoutSeconds -Condition {
                    $s = Get-GameIntrospection -Serial $EMU1 -Fresh
                    return $s -and $s.is_network -and $s.multiplayer.num_connected -eq 1
                })) { throw 'Host lobby did not start' }
        $callsigns = @($CALLSIGN2, 'LanThird')
        for ($i = 1; $i -lt 3; $i++) {
            Send-MpCommand -Serial $serials[$i] -Command 'lan_launch' -Extras ($common + @('--es', 'mp_mode', 'join',
                    '--es', 'callsign', $callsigns[$i - 1], '--es', 'host_addr', $hostIp, '--ei', 'host_port', '42424'))
        }
        if (-not (Wait-ForCondition -Description 'All three peers enter the mine' -TimeoutSec $TimeoutSeconds -Condition {
                    foreach ($serial in $serials) {
                        $states[$serial] = Get-GameIntrospection -Serial $serial -Fresh
                        $s = $states[$serial]
                        if (-not $s -or -not $s.in_game -or $s.multiplayer.num_connected -ne 3) { return $false }
                    }
                    return $true
                })) { throw 'Three-player synchronization failed' }
        $slots = @($serials | ForEach-Object { [int]$states[$_].multiplayer.my_player_num })
        if ($slots[0] -ne 0 -or @($slots | Select-Object -Unique).Count -ne 3) { throw 'Unexpected three-player slots' }
        $initialVisit = [uint64]$states[$EMU1].coop_world_visit.active
        if ($PickupRelayCase -eq 'restore') {
            if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName 'test_coop_level_restart_remember.jsonc') -or
                -not (Wait-PickupRelayAutomation -Serials @($EMU1) -Description 'Retain the level-entry checkpoint before creating drops')) { return $false }
        }

        # Clear combat on every peer, then create one real death-spew laser
        foreach ($serial in $serials) {
            if (-not (Start-DeviceGameAutomation -Serial $serial -ScriptName 'test_coop_host_migration_prepare_world.jsonc')) { return $false }
        }
        if (-not (Wait-PickupRelayAutomation -Serials $serials -Description 'Prepare three-player mine')) { return $false }
        foreach ($serial in @($EMU1, $PickupRelayDevice)) {
            if (-not (Start-DeviceGameAutomation -Serial $serial -ScriptName 'test_coop_pickup_race_host.jsonc')) { return $false }
        }
        if (-not (Start-DeviceGameAutomation -Serial $EMU2 -ScriptName 'test_coop_pickup_race_client.jsonc')) { return $false }
        if (-not (Wait-PickupRelayAutomation -Serials $serials -Description 'Create one laser for two nonhost collectors')) { return $false }
        $collectors = @($EMU2, $PickupRelayDevice)
        foreach ($serial in $collectors) {
            if (-not (Start-DeviceGameAutomation -Serial $serial -ScriptName 'test_coop_pickup_race_arm.jsonc')) { return $false }
        }
        if (-not (Wait-PickupRelayAutomation -Serials $collectors -Description 'Hold both clients pickup requests')) { return $false }
        $pendingRecovery = (Get-GameIntrospection -Serial $EMU1 -Fresh).multiplayer.recovery
        if (-not $pendingRecovery -or $pendingRecovery.live -lt 1) { throw 'Missing live recovery gear before pickup resolution' }
        if ($PickupRelayCase -eq 'restore') {
            foreach ($serial in $collectors) {
                if (-not (Start-DeviceGameAutomation -Serial $serial -ScriptName 'test_coop_pickup_restore_wait.jsonc')) { return $false }
            }
            if (-not (Start-DeviceGameAutomation -Serial $EMU1 -ScriptName 'test_coop_pickup_restore_host.jsonc')) { return $false }
            if (-not (Wait-PickupRelayAutomation -Serials $serials -Description 'Restore while both clients have unawarded requests' -TimeoutSec 240)) { return $false }
        } elseif ($PickupRelayCase -eq 'travel') {
            foreach ($serial in $serials) {
                if (-not (Start-DeviceGameAutomation -Serial $serial -ScriptName 'test_coop_pickup_travel.jsonc')) { return $false }
            }
            if (-not (Wait-PickupRelayAutomation -Serials $serials -Description 'Leave the level with both pickup requests pending' -TimeoutSec 240)) { return $false }
        } else {
            foreach ($serial in $collectors) {
                if (-not (Start-DeviceGameAutomation -Serial $serial -ScriptName 'test_coop_pickup_relay_release.jsonc')) { return $false }
            }
            if (-not (Wait-PickupRelayAutomation -Serials $collectors -Description 'Relay competing requests through the host')) { return $false }
        }
        $total = 0
        foreach ($serial in $serials) {
            $s = Get-GameIntrospection -Serial $serial -Fresh
            if (-not $s -or -not $s.in_game -or $s.multiplayer.num_connected -ne 3 -or -not $s.multiplayer.recovery.ready_to_save) {
                throw "Unsettled pickup or disconnected peer on $serial"
            }
            $total += [int]$s.player.laser_level
            if ($serial -eq $EMU1 -and $s.player.laser_level -ne 0) { throw 'Noncollecting host gained a laser' }
            if ($PickupRelayCase -eq 'restore' -and
                ([uint64]$s.coop_world_visit.active -le $initialVisit -or $s.multiplayer.recovery.rows -ne 0)) {
                throw "$serial retained the old world or its drops after checkpoint restore"
            }
            if ($PickupRelayCase -eq 'travel' -and
                ($s.current_level_num -ne $InitialLevel + 1 -or [uint64]$s.coop_world_visit.active -le $initialVisit -or
                $s.multiplayer.recovery.live -ne 0 -or $s.multiplayer.recovery.dormant -ne 0 -or
                $s.multiplayer.recovery.rows -ne $pendingRecovery.rows -or
                $s.multiplayer.recovery.credit -ne $pendingRecovery.live + $pendingRecovery.credit)) {
                throw "$serial failed to carry abandoned drops into recovery credit in the next level"
            }
            $s | ConvertTo-Json -Depth 40 | Set-Content -LiteralPath "$output/$serial-final.json" -Encoding utf8
        }
        if ($PickupRelayCase -eq 'restore') {
            if ($total -ne 0) { throw 'Restored ships received an award from an abandoned pickup request' }
            Write-Status 'Checkpoint restore cancelled both pending pickups with no inventory award or retained old drops' 'Green'
            return $true
        }
        if ($PickupRelayCase -eq 'travel') {
            if ($total -ne 0) { throw 'Level travel awarded a cancelled pickup' }
            Write-Status 'Level travel cancelled both pending pickups and retained the abandoned drops as recovery credit' 'Green'
            return $true
        }
        if ($total -ne 1) { throw "One dropped laser awarded $total levels across three players" }

        # The clients must see each other's logical sender through the host relay
        for ($i = 1; $i -lt 3; $i++) {
            $serial = $serials[$i]
            $peerSlot = $slots[3 - $i]
            $log = Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-d', '-s', 'DXX-DLOG:*', '*:S') -Seconds 10
            # REC_PICKUP_REQUEST=8 and REC_PICKUP_REPLY=9 in coop_recovery.c
            if ($log -notmatch "receive sender=$peerSlot master=0 operation=8 " -or
                $log -notmatch "receive sender=$peerSlot master=0 operation=9 ") {
                throw "$serial did not receive the other client's pickup request and permission"
            }
        }
        Write-Status 'Exactly one laser awarded between two clients through the three-player host relay' 'Green'
        return $true
    } finally {
        foreach ($capture in $captures) {
            if (-not $capture.HasExited) { Stop-Process -Id $capture.Id -Force }
        }
        foreach ($serial in $serials) {
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) -Seconds 5 | Out-Null
        }
    }
}

function Wait-PickupRelayAutomation {
    param([string[]]$Serials, [string]$Description, [int]$TimeoutSec = 60)
    $results = @{}
    $done = Wait-ForCondition -Description $Description -TimeoutSec $TimeoutSec -PollMs 500 -Condition {
        foreach ($serial in $Serials) {
            $results[$serial] = Get-DeviceAutomationResult -Serial $serial
            if (-not $results[$serial] -or $results[$serial].result -notin @('PASS', 'FAIL')) { return $false }
        }
        return $true
    }
    foreach ($serial in $Serials) {
        if (-not $done -or $results[$serial].result -ne 'PASS') {
            Write-DeviceAutomationDiagnostics -Serial $serial
            return $false
        }
    }
    return $true
}
