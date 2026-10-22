#!/usr/bin/env pwsh
# test_mp.ps1 -- Two-player multiplayer integration test.
#
# Orchestrates two Android emulator instances and a matchmaking server to:
#   1. Launch SetupActivity on both emulators
#   2. Connect both players to the server via MP_COMMAND
#   3. Player 1 creates a lobby, Player 2 joins
#   4. Both players ready up
#   5. Player 1 starts the game
#   6. Both players launch into the game and verify multiplayer state
#   7. Optional sustained connectivity soak
#
# Prerequisites:
#   - Two emulators running (emulator-5554 and emulator-5556)
#   - APK installed on both
#   - Game data present on both
#   - Matchmaking server NOT running (this script starts it)
#
# Usage:
#   .\test_mp.ps1
#   .\test_mp.ps1 -Game d1
#   .\test_mp.ps1 -SkipBuild
#   .\test_mp.ps1 -SoakSeconds 0   # normal-suite smoke profile

param(
    [string]$Game = "d2",
    [string]$Mode = "anarchy",
    [switch]$SkipBuild,
    [switch]$PublicShareware,
    [ValidateSet("d1", "d2")][string]$PublicGame = "d1",
    [switch]$PublicDirect,
    [ValidateSet('', 'ServerDirect', 'AuthenticatedRelay', 'InitialFallback')][string]$TransportMode = '',
    [switch]$Tls,
    [int]$TimeoutSeconds = 180,
    [ValidateRange(0, 3600)]
    [int]$SoakSeconds = 90
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Source shared env setup (JAVA_HOME, cmake, cargo)
. "$PSScriptRoot\..\helpers\test_env.ps1"
. "$PSScriptRoot\..\helpers\test_helpers.ps1"

if ($TransportMode -and (-not $PublicShareware -or $PublicDirect)) {
    throw 'TransportMode requires PublicShareware and selects its own direct/relay policy'
}
if ($TransportMode) { $PublicDirect = $TransportMode -eq 'ServerDirect' }
. "$PSScriptRoot/../helpers/cooperative_gameplay.ps1"
. "$PSScriptRoot/../helpers/online_transport_modes.ps1"

# -- Constants --
$REPO_ROOT = Split-Path (Split-Path $PSScriptRoot)
$DEP_BASE = (Get-Content (Join-Path $REPO_ROOT "dependency_base.txt") -First 1).Trim()
$ADB = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "platform-tools" -ToolName "adb" -EnvironmentVariable "ADB"
$PACKAGE = "com.dxxredux.app"
$ACTIVITY = "com.dxxredux.app.SetupActivity"

$EMULATOR = Resolve-RegressionAndroidSdkTool -DepBase $DEP_BASE -Subdir "emulator" -ToolName "emulator"
$EMU1 = $script:PRIMARY_EMULATOR_SERIAL  # Player 1 (host)
$EMU2 = $script:SECONDARY_EMULATOR_SERIAL  # Player 2 (joiner)
$AVD_MAP = @{ $EMU1 = $script:PRIMARY_AVD_NAME; $EMU2 = $script:SECONDARY_AVD_NAME }
$CALLSIGN1 = "HostPlt"
$CALLSIGN2 = "JoinPlt"
$EMU3 = 'emulator-5558'
$CALLSIGN3 = 'LatePlt'
$allTargets = @($EMU1, $EMU2)
if ($PublicShareware -and -not $TransportMode) { $allTargets += $EMU3 }

if ($PublicShareware) {
    $Game = $PublicGame
    $Mode = if ($TransportMode) { 'coop' } else { 'anarchy' }
    $SoakSeconds = 15
    . "$PSScriptRoot/../helpers/shareware_test_data.ps1"
    $publicConfig = Get-SharewareTestGameConfig -RepoRoot $REPO_ROOT -Game $Game
    $publicDirectory = Resolve-SharewareTestData -RepoRoot $REPO_ROOT -Game $Game -DataDir ''
}
$MISSION = if ($Game -eq "d1") { "descent" } else { "d2" }
if ($PublicShareware) { $MISSION = if ($Game -eq 'd1') { "''" } else { $publicConfig.Mission } }
$MODE = $Mode

$serverProcess = $null
$faultProcess = $null
$faultControl = Join-Path $REPO_ROOT "temp/online-fault-control.json"
$testPassed = $false

$script:LogFile = Join-Path $REPO_ROOT "temp\mp_test_log.txt"
try { if (Test-Path $script:LogFile) { Remove-Item $script:LogFile -Force -ErrorAction SilentlyContinue } } catch { }
try { [IO.File]::WriteAllText($script:LogFile, [Environment]::NewLine, [Text.UTF8Encoding]::new($false)) } catch {}

function Get-MpIntrospection {
    param([string]$Serial)
    # Remove the previous snapshot so a delayed broadcast cannot satisfy this
    # poll with stale lobby state.
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
        "shell", "run-as", $PACKAGE, "rm", "-f", "files/mp_introspect.json"
    ) -Seconds 10 | Out-Null
    Send-MpCommand -Serial $Serial -Command "introspect"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 3) {
        $json = Adb-Dev-Timeout -Serial $Serial -AdbArgs @(
            "shell", "run-as", $PACKAGE, "cat", "files/mp_introspect.json"
        ) -Seconds 10
        if ($json -and $json -match '"status"') {
            try {
                return $json | ConvertFrom-Json
            } catch {
                return $null
            }
        }
        Start-Sleep -Milliseconds 100
    }
    return $null
}

function Measure-InstallationWork {
    param([string]$ServerUrl)
    # Only called after disposable emulator validation, against our in-memory server
    $failed = 0
    $sample = 0
    foreach ($serial in $allTargets) {
        for ($trial = 0; $trial -lt 4; $trial++) {
            $sample++
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) -Seconds 10 | Out-Null
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'run-as', $PACKAGE, 'rm', '-rf', 'no_backup/online_identity') -Seconds 10 | Out-Null
            if (-not (Start-SetupActivity -Serial $serial)) { throw 'Work measurement launcher unavailable' }
            Send-MpCommand -Serial $serial -Command 'set_callsign' -Extras @('--es', 'callsign', 'WorkProbe')
            Send-MpCommand -Serial $serial -Command 'connect' -Extras @('--es', 'url', $ServerUrl)
            $done = Wait-ForCondition -Description "Installation work sample $sample" -TimeoutSec 50 -PollMs 500 -Condition {
                $snapshot = Get-MpIntrospection -Serial $serial
                return $snapshot -and ($snapshot.status -eq 'CONNECTED' -or $snapshot.error)
            }
            $snapshot = Get-MpIntrospection -Serial $serial
            $work = if ($snapshot) { $snapshot.online_resources.installation_work } else { $null }
            $valid = $done -and $snapshot.status -eq 'CONNECTED' -and $work -and
            $work.PSObject.Properties['difficulty'] -and $work.difficulty -eq 20 -and
            $work.PSObject.Properties['attempt'] -and $work.attempt -ge 1 -and $work.attempt -le 3 -and
            $work.hashes -gt 0 -and $work.solved
            if (-not $valid) { $failed++ }
            if ($work -and $work.PSObject.Properties['difficulty']) {
                Write-Status "ONLINE_WORK sample=$sample difficulty=$([int]$work.difficulty) hashes=$([long]$work.hashes) elapsed_ms=$([long]$work.elapsed_ms) solved=$([bool]$work.solved) authenticated=$([bool]$valid) attempts=$([int]$work.attempt)" 'Gray'
            } else {
                Write-Status "ONLINE_WORK sample=$sample category=missing-measurement authenticated=False" 'Gray'
            }
            Send-MpCommand -Serial $serial -Command 'disconnect'
            if (-not (Wait-ForCondition -Description 'Work sample socket cleanup' -TimeoutSec 10 -PollMs 250 -Condition {
                        $ready = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2
                        return $ready.pending_and_authenticated_sockets -eq 0 -and $ready.rooms -eq 0 -and $ready.relay_sessions -eq 0
                    })) { throw 'Work measurement leaked server resources' }
        }
    }
    $vectorFailures = 0
    foreach ($serial in $allTargets) {
        $output = Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'instrument', '-w', '-r', '-e', 'suite', 'installation_work', "$PACKAGE.test/com.dxxredux.app.RecoveryInstrumentation") -Seconds 35
        foreach ($line in @($output -split "`n")) {
            if ($line -match 'ONLINE_WORK_VECTOR difficulty=20 hashes=[0-9]+ elapsed_ms=[0-9]+ solved=(true|false)|ONLINE_WORK_(EXHAUSTED|CANCELLED) hashes=512 solved=false') {
                Write-Status "$($Matches[0])" 'Gray'
            }
        }
        $passed = $output -match 'PASS: installation work fixture and cancellation'
        $finished = $output -match '(?m)^INSTRUMENTATION_CODE: -1\s*$'
        Write-Status "ONLINE_WORK_RESULT pass_marker=$([bool]$passed) terminal_ok=$([bool]$finished)" 'Gray'
        if (-not $passed -or -not $finished) { $vectorFailures++ }
    }
    if ($failed -gt 0 -or $vectorFailures -gt 0) {
        throw "Installation work qualification failed: $failed of $sample fresh samples; $vectorFailures of 3 device vectors"
    }
    foreach ($serial in $allTargets) {
        if (-not (Start-SetupActivity -Serial $serial)) { throw 'Launcher unavailable after work vector' }
    }
    Write-Status "ONLINE_EVIDENCE installation work authenticated $sample fresh identities at difficulty 20 and passed 3 expensive device vectors" 'Green'
}

function Test-OnlineDirectProbe {
    param($Control, [string]$RoomId, [string]$PlayerId, [string]$PeerId,
        [string[]]$CandidateTypes = @('host', 'srflx', 'observed', 'upnp', 'predicted'))
    if (-not $Control -or -not $Control.PSObject.Properties['lobby'] -or -not $Control.lobby -or
        $Control.lobby.lobby_id -cne $RoomId -or $Control.player_id -cne $PlayerId -or
        -not $Control.PSObject.Properties['online_resources']) { return $false }
    $roster = @($Control.lobby.players | ForEach-Object { $_.player_id })
    if ($roster.Count -ne 2 -or $PlayerId -ceq $PeerId -or $PlayerId -cnotin $roster -or $PeerId -cnotin $roster) { return $false }
    $resources = $Control.online_resources
    if (-not $resources.PSObject.Properties['connectivity']) { return $false }
    $probe = $resources.connectivity
    foreach ($field in @('network_epoch', 'check_epoch', 'phase', 'results')) {
        if (-not $probe -or -not $probe.PSObject.Properties[$field]) { return $false }
    }
    if ($probe.phase -cne 'COMPLETE' -or $probe.network_epoch -le 0 -or $probe.check_epoch -le 0) { return $false }
    $results = @($probe.results)
    if ($results.Count -ne 1) { return $false }
    $result = $results[0]
    foreach ($field in @('room_id', 'network_epoch', 'check_epoch', 'peer_id', 'candidate_type')) {
        if (-not $result -or -not $result.PSObject.Properties[$field]) { return $false }
    }
    return ($result.room_id -ceq $RoomId -and $result.network_epoch -eq $probe.network_epoch -and
        $result.check_epoch -eq $probe.check_epoch -and $result.peer_id -ceq $PeerId -and
        $result.candidate_type -cin $CandidateTypes)
}

function Wait-OnlineDirectReadiness {
    $hostControl = Get-MpIntrospection -Serial $EMU1
    $joinControl = Get-MpIntrospection -Serial $EMU2
    if (-not $hostControl -or -not $joinControl -or -not $hostControl.lobby -or -not $joinControl.lobby -or
        $hostControl.lobby.lobby_id -cne $joinControl.lobby.lobby_id) { throw 'Direct fixture has no shared current room' }
    $script:directRoom = [string]$hostControl.lobby.lobby_id
    $identities = @([string]$hostControl.player_id, [string]$joinControl.player_id)
    $script:directIdentities = $identities
    $wlanIps = @((Get-DeviceWlanIp -Serial $EMU1), (Get-DeviceWlanIp -Serial $EMU2))
    $reportedChecks = [Collections.Generic.HashSet[string]]::new()
    if (-not $script:directRoom -or -not $identities[0] -or -not $identities[1] -or $identities[0] -ceq $identities[1]) {
        throw 'Direct fixture has no distinct current identities'
    }
    $complete = Wait-ForCondition -Description 'Both current peer probes complete directly' -TimeoutSec 30 -PollMs 500 -Condition {
        $passed = $true
        $serials = @($EMU1, $EMU2)
        for ($i = 0; $i -lt 2; $i++) {
            $control = Get-MpIntrospection -Serial $serials[$i]
            $accepted = Test-OnlineDirectProbe -Control $control -RoomId $script:directRoom -PlayerId $identities[$i] -PeerId $identities[1 - $i]
            # Bounded, address/key/token-free evidence retained before logcat is cleared
            $phase = 'unavailable'; $epoch = 0; $check = 0; $candidates = 0; $peerCandidates = 0; $methods = 'none'
            if ($control -and $control.PSObject.Properties['online_resources'] -and
                $control.online_resources.PSObject.Properties['connectivity']) {
                $probe = $control.online_resources.connectivity
                if ($probe.phase -cin @('IDLE', 'STUN_DISCOVERY', 'STUN_COMPLETE', 'PROBING', 'COMPLETE', 'FAILED')) { $phase = $probe.phase }
                $epoch = [long]$probe.network_epoch; $check = [long]$probe.check_epoch
                $candidates = [int]$probe.candidate_count; $peerCandidates = [int]$probe.peer_candidate_count
                $methods = (@($probe.results | ForEach-Object { $_.candidate_type } |
                            Where-Object { $_ -cin @('host', 'srflx', 'observed', 'upnp', 'predicted', 'relay') } | Select-Object -First 2) -join ',')
            }
            if ($phase -eq 'COMPLETE' -and $probe.PSObject.Properties['probe_counts'] -and
                $reportedChecks.Add("${i}/${epoch}/${check}")) {
                $localWifi = $wlanIps[$i] -and @($probe.local_candidates | Where-Object { $_.type -ceq 'host' -and $_.addr.StartsWith("$($wlanIps[$i]):") }).Count -gt 0
                $peerWifi = $wlanIps[1 - $i] -and @($probe.pairs | Where-Object { $_.type -ceq 'host' -and $_.addr.StartsWith("$($wlanIps[1 - $i]):") }).Count -gt 0
                $wifiSends = 0
                for ($pairIndex = 0; $pairIndex -lt @($probe.pairs).Count; $pairIndex++) {
                    $pair = @($probe.pairs)[$pairIndex]
                    $sent = $probe.probe_counts.PSObject.Properties["sent_$pairIndex"]
                    if ($wlanIps[1 - $i] -and $pair.addr.StartsWith("$($wlanIps[1 - $i]):") -and $sent) {
                        $wifiSends += [int]$sent.Value
                    }
                }
                $counts = @($probe.probe_counts.PSObject.Properties | Where-Object {
                        $_.Name -match '^(sent_[0-9]{1,2}|send_errors|received|requests|echoes|echo_errors|responses|matched|short_packets|other_packets|wrong_source|wrong_source_offered|wrong_source_unoffered|self_requests|echoes_after_match)$'
                    } | Select-Object -First 48 | ForEach-Object { "$($_.Name)=$([int]$_.Value)" }) -join ','
                Write-Status "ONLINE_PROBE peer=$i local_wifi_advertised=$localWifi peer_wifi_offered=$peerWifi wifi_sends=$wifiSends counts={$counts}" 'Gray'
            }
            Write-Status "ONLINE_DIRECT peer=$i accepted=$accepted phase=$phase network_epoch=$epoch check_epoch=$check candidates=$candidates peer_candidates=$peerCandidates methods=$methods" 'Gray'
            $passed = $passed -and $accepted
        }
        return $passed
    }
    if (-not $complete) { throw 'Direct qualification failed: current pair probes did not complete directly; game was not started' }
}

function Test-AllLobbyPlayersReady {
    param([string]$Serial, [int]$ExpectedPlayers = 2, [switch]$RequireDirect)

    $mp = Get-MpIntrospection -Serial $Serial
    if (-not $mp -or -not $mp.PSObject.Properties['lobby'] -or -not $mp.lobby) {
        return $false
    }

    $playersProperty = $mp.lobby.PSObject.Properties['players']
    if (-not $playersProperty -or $null -eq $playersProperty.Value) {
        return $false
    }

    $players = @($playersProperty.Value)
    if ($RequireDirect -and ($mp.lobby.lobby_id -cne $script:directRoom -or $players.Count -ne $ExpectedPlayers)) { return $false }
    if ($players.Count -lt $ExpectedPlayers) {
        return $false
    }

    foreach ($player in $players) {
        if ($RequireDirect -and $player.player_id -cnotin $script:directIdentities) { return $false }
        if ($RequireDirect -and (-not $player.PSObject.Properties['connection_type'] -or
                -not $player.PSObject.Properties['ping_ms'] -or $null -eq $player.ping_ms -or $player.ping_ms -lt 0 -or
                $player.connection_type -cnotin @('direct_lan', 'direct_upnp', 'direct_holepunch', 'predicted_holepunch'))) { return $false }
        $readyProperty = $player.PSObject.Properties['ready']
        if (-not $readyProperty -or -not [bool]$readyProperty.Value) {
            return $false
        }
    }

    return $true
}

function Get-OnlinePeerSample {
    param([string[]]$Serials, [string[]]$Callsigns, [int[]]$Slots, [string]$Phase, [hashtable]$Baseline)
    $allValid = $true
    foreach ($serial in $Serials) {
        $game = Get-GameIntrospection -Serial $serial
        $control = Get-MpIntrospection -Serial $serial
        $nativeReady = $game -and $game.in_game -and $game.is_network
        $roomReady = $control -and $control.PSObject.Properties['lobby']
        $peers = @(if ($control) { $control.online_resources.peers })
        $connected = @(if ($game -and $game.PSObject.Properties['multiplayer'] -and $game.multiplayer) {
                $game.multiplayer.players | Where-Object { $_.connected -eq 1 } | ForEach-Object { $_.callsign.ToLowerInvariant() } | Sort-Object
            })
        $pending = $game -and $game.PSObject.Properties['multiplayer'] -and $game.multiplayer -and
        $game.multiplayer.PSObject.Properties['join_request_pending'] -and $game.multiplayer.join_request_pending
        $nativeSlot = -1
        $networkStatus = -1
        $pendingFixture = -1
        if ($game -and $game.PSObject.Properties['multiplayer'] -and $game.multiplayer) {
            if ($game.multiplayer.PSObject.Properties['my_player_num']) { $nativeSlot = [int]$game.multiplayer.my_player_num }
            if ($game.multiplayer.PSObject.Properties['network_status']) { $networkStatus = [int]$game.multiplayer.network_status }
            if ($pending -and $game.multiplayer.PSObject.Properties['join_request_callsign']) {
                for ($fixture = 0; $fixture -lt $Callsigns.Count; $fixture++) {
                    if ($game.multiplayer.join_request_callsign -ieq $Callsigns[$fixture]) { $pendingFixture = $fixture }
                }
            }
        }
        $expected = @($Callsigns | ForEach-Object { $_.ToLowerInvariant() } | Sort-Object)
        $rosterReady = ($connected -join ',') -ceq ($expected -join ',')
        $peerSummary = (@($peers | ForEach-Object { "$([int]$_.slot):$([bool]$_.bound):$([long]$_.sent):$([long]$_.gameplay_received)" })) -join ','
        Write-Status "ONLINE_STATE $Phase $serial native=$([bool]$nativeReady) room=$([bool]$roomReady) roster_ok=$rosterReady pending_approval=$([bool]$pending) pending_fixture=$pendingFixture native_slot=$nativeSlot network_status=$networkStatus connected=$($connected.Count) peers=$peerSummary" 'Gray'
        if (-not $nativeReady -or -not $roomReady -or -not $rosterReady) { $allValid = $false; continue }
        $self = @($control.lobby.players | Where-Object { $_.player_id -eq $control.player_id })
        if ($self.Count -ne 1) { $allValid = $false; continue }
        $ownSlot = $self[0].slot
        Write-Status "ONLINE_SAMPLE $Phase $serial connected=$($connected -join ',') own=$ownSlot peers=$((@($peers | ForEach-Object { "$($_.slot):$($_.gameplay_received)" })) -join ',')" 'Gray'
        $expectedPeers = @($Slots | Where-Object { $_ -ne $ownSlot } | Sort-Object)
        if ((@($peers.slot | Sort-Object) -join ',') -ne ($expectedPeers -join ',')) { $allValid = $false; continue }
        foreach ($peer in $peers) {
            if (-not $peer.bound -or $peer.sent -le 0 -or $peer.gameplay_received -le 0) { $allValid = $false; continue }
            $key = "$serial/$($peer.slot)"
            if (-not $baseline.ContainsKey($key)) {
                $baseline[$key] = @{ Sent = $peer.sent; Received = $peer.gameplay_received }
                $allValid = $false
                continue
            }
            if ($peer.sent -le $baseline[$key].Sent -or $peer.gameplay_received -le $baseline[$key].Received) { $allValid = $false }
        }
    }
    return $allValid
}

function Assert-OnlinePeerMatrix {
    param([string[]]$Serials, [string[]]$Callsigns, [int[]]$Slots, [string]$Phase)
    $baseline = @{}
    $valid = Wait-ForCondition -Description $Phase -TimeoutSec 45 -PollMs 1000 -Condition {
        Get-OnlinePeerSample -Serials $Serials -Callsigns $Callsigns -Slots $Slots -Phase $Phase -Baseline $baseline
    }
    if (-not $valid) { throw "Native online peer matrix failed: $Phase" }
    Write-Status "ONLINE_EVIDENCE $Phase clients=$($Serials.Count) slots=$($Slots -join ',') every peer gameplay counter advanced" 'Green'
}

function Test-ExpectedOnlineAdmission {
    param($GameState, $Control, [string]$PlayerId, [string]$Callsign, [int]$Slot)
    if (-not $GameState -or -not $GameState.PSObject.Properties['multiplayer'] -or -not $GameState.multiplayer -or
        -not $Control -or -not $Control.PSObject.Properties['lobby']) { return $false }
    $native = $GameState.multiplayer
    if ([string]::IsNullOrWhiteSpace($PlayerId) -or $Slot -lt 1) { return $false }
    foreach ($field in @('i_am_master', 'join_request_pending', 'join_request_callsign', 'join_request_age_ms')) {
        if (-not $native.PSObject.Properties[$field]) { return $false }
    }
    $member = @($Control.lobby.players | Where-Object { $_.player_id -ceq $PlayerId -and $_.callsign -ieq $Callsign -and $_.slot -eq $Slot })
    return $GameState.in_game -and $GameState.is_network -and $native.i_am_master -and
    $native.join_request_pending -and $native.join_request_callsign -ieq $Callsign -and
    $native.join_request_age_ms -ge 0 -and $native.join_request_age_ms -lt 6000 -and $member.Count -eq 1
}

function Assert-OnlineAdmissionCancelled {
    param([string]$Serial, [switch]$NativeTimeout)
    # Native timeout must clean membership with the authenticated control socket alive
    if (-not $NativeTimeout) { Send-MpCommand -Serial $Serial -Command 'disconnect' }
    $expectedSockets = if ($NativeTimeout) { 3 } else { 2 }
    $expectedPairs = if ($PublicDirect) { 0 } else { 1 }
    if (-not (Wait-ForCondition -Description 'Unapproved join terminal retires only joining client' -TimeoutSec 25 -PollMs 500 -Condition {
                $ready = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2
                $control = Get-MpIntrospection -Serial $Serial
                if (-not $control) { return $false }
                $r = $control.online_resources
                return $ready.rooms -eq 1 -and $ready.relay_sessions -eq $expectedPairs -and $ready.pending_and_authenticated_sockets -eq $expectedSockets -and
                -not $control.PSObject.Properties['lobby'] -and $r.proxy_workers -eq 0 -and $r.background_jobs -eq 0 -and
                -not $r.candidate_socket -and -not $r.upnp_mapping -and @($r.peers).Count -eq 0
            })) { throw 'Unapproved join cancellation left membership, routes or workers' }
    if ($NativeTimeout) {
        $nativeLog = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('logcat', '-d') -Seconds 10
        if ($nativeLog -notmatch 'ONLINE_ADMISSION event=host_refusal') { throw 'Missing real native refusal/timeout producer receipt' }
        $hostLog = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('logcat', '-d') -Seconds 10
        $hostTimeouts = [regex]::Matches(($hostLog -join "`n"), 'ONLINE_ADMISSION event=host_consent_timeout').Count
        if ($hostTimeouts -le $script:onlineHostTimeoutCount) { throw 'No new native host consent timeout receipt' }
        Write-Status 'ONLINE_EVIDENCE native refusal/timeout cleaned assigned join without client cancel or F6' 'Green'
    }
    # Let the native consent prompt and retired-peer relay timeout expire without approval
    Start-Sleep -Seconds 12
    $hostState = Get-GameIntrospection -Serial $EMU1
    if (-not $hostState -or $hostState.multiplayer.join_request_pending) { throw 'Cancelled admission left a pending native request' }
    Assert-OnlinePeerMatrix -Serials @($EMU1, $EMU2) -Callsigns @($CALLSIGN1, $CALLSIGN2) -Slots @(0, 1) -Phase 'unapproved join cancellation preserves original pair'
    # Cleanup is already proved; stopping the unused native join screen cannot mask a leak
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) -Seconds 10 | Out-Null
    $terminal = if ($NativeTimeout) { 'timed out natively' } else { 'cancelled' }
    Write-Status "ONLINE_EVIDENCE expected native request $terminal without F6; original pair and room preserved" 'Green'
}

function Join-RunningOnlineGame {
    param([string]$Serial, [string]$Callsign, [switch]$CancelAdmission, [switch]$AllowKnownRejoin, [switch]$NativeTimeout, [ValidateRange(0, 45)][int]$DelayLaunchSeconds = 0)
    if ($NativeTimeout) {
        Adb-Dev-Timeout -Serial $Serial -AdbArgs @('logcat', '-c') -Seconds 10 | Out-Null
        $hostLog = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('logcat', '-d') -Seconds 10
        $script:onlineHostTimeoutCount = [regex]::Matches(($hostLog -join "`n"), 'ONLINE_ADMISSION event=host_consent_timeout').Count
    }
    if (-not (Start-SetupActivity -Serial $Serial)) { throw "Late join launcher failed on $Serial" }
    Send-MpCommand -Serial $Serial -Command 'set_callsign' -Extras @('--es', 'callsign', $Callsign)
    Send-MpCommand -Serial $Serial -Command 'connect' -Extras @('--es', 'url', $testServerUrl)
    if (-not (Wait-ForCondition -Description 'Late join authenticated' -TimeoutSec 50 -PollMs 500 -Condition {
                $control = Get-MpIntrospection -Serial $Serial
                return $control -and $control.status -eq 'CONNECTED'
            })) { throw 'Late join authentication failed' }
    Send-MpCommand -Serial $Serial -Command 'refresh_lobbies'
    if (-not (Wait-ForCondition -Description 'Running room visible' -TimeoutSec 15 -PollMs 500 -Condition {
                $control = Get-MpIntrospection -Serial $Serial
                return $control -and @($control.lobbies | Where-Object { $_.joinable }).Count -eq 1
            })) { throw 'Running room unavailable for late join' }
    Send-MpCommand -Serial $Serial -Command 'join_first_lobby'
    if (-not (Wait-ForCondition -Description 'Late join launch assignment' -TimeoutSec 30 -PollMs 500 -Condition {
                $control = Get-MpIntrospection -Serial $Serial
                return $control -and $control.game_launch_pending
            })) { throw 'Late join did not receive launch assignment' }
    $joining = Get-MpIntrospection -Serial $Serial
    $playerId = $joining.player_id
    $self = @($joining.lobby.players | Where-Object { $_.player_id -ceq $playerId })
    if ($self.Count -ne 1) { throw 'Joining identity has no unique stable room slot' }
    $slot = [int]$self[0].slot
    # The existing LAN approval producer is a real F6 key; this fixture has the online owner
    $scriptName = 'test_online_late_join_accept.jsonc'
    $scriptPath = Join-Path $REPO_ROOT "android/game_scripts/$scriptName"
    Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('push', $scriptPath, "/data/local/tmp/$scriptName") -Seconds 10 | Out-Null
    Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'run-as', $PACKAGE, 'cp', "/data/local/tmp/$scriptName", "files/$scriptName") -Seconds 10 | Out-Null
    Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'run-as', $PACKAGE, 'rm', '-f', 'files/automation_result.json') -Seconds 10 | Out-Null
    if ($DelayLaunchSeconds -gt 0) {
        Start-Sleep -Seconds $DelayLaunchSeconds
        $pending = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2
        if ($pending.pending_native_admissions -ne 1) { throw 'Delayed unlaunched admission was not retained' }
    }
    Send-MpCommand -Serial $Serial -Command 'launch_game'
    $script:onlineKnownRejoin = $false
    $observed = Wait-ForCondition -Description 'Expected native host consent request or authenticated known rejoin' -TimeoutSec 35 -PollMs 500 -Condition {
        $hostState = Get-GameIntrospection -Serial $EMU1
        $hostControl = Get-MpIntrospection -Serial $EMU1
        $expected = Test-ExpectedOnlineAdmission -GameState $hostState -Control $hostControl -PlayerId $playerId -Callsign $Callsign -Slot $slot
        $pending = $hostState -and $hostState.PSObject.Properties['multiplayer'] -and $hostState.multiplayer -and $hostState.multiplayer.join_request_pending
        Write-Status "ONLINE_ADMISSION native_pending=$([bool]$pending) expected_request=$expected slot=$slot" 'Gray'
        if ($expected) { return $true }
        $member = @()
        if ($hostControl -and $hostControl.PSObject.Properties['lobby']) {
            $member = @($hostControl.lobby.players | Where-Object { $_.player_id -ceq $playerId -and $_.callsign -ieq $Callsign -and $_.slot -eq $slot })
        }
        if ($AllowKnownRejoin -and $member.Count -eq 1 -and -not $CancelAdmission -and $hostState -and $hostState.in_game -and $hostState.is_network -and
            $hostState.multiplayer.i_am_master -and @($hostState.multiplayer.players | Where-Object { $_.connected -eq 1 -and $_.callsign -ieq $Callsign }).Count -eq 1) {
            $script:onlineKnownRejoin = $true
            return $true
        }
        return $false
    }
    if (-not $observed) {
        Get-OnlinePeerSample -Serials $allTargets -Callsigns @($CALLSIGN1, $CALLSIGN2, $CALLSIGN3) -Slots @(0, 1, 2) -Phase 'admission observation failure' -Baseline @{} | Out-Null
        throw 'Expected native consent or known rejoin was not observed'
    }
    if ($NativeTimeout) { Assert-OnlineAdmissionCancelled -Serial $Serial -NativeTimeout; return }
    if ($CancelAdmission) { Assert-OnlineAdmissionCancelled -Serial $Serial; return }
    if ($script:onlineKnownRejoin) { Write-Status 'ONLINE_EVIDENCE known native identity rejoined without a new consent prompt' 'Green'; return }
    # Recheck immediately before sending the key; never approve an unrelated or expired request
    $hostControl = Get-MpIntrospection -Serial $EMU1
    $hostState = Get-GameIntrospection -Serial $EMU1
    if (-not (Test-ExpectedOnlineAdmission -GameState $hostState -Control $hostControl -PlayerId $playerId -Callsign $Callsign -Slot $slot)) {
        Get-OnlinePeerSample -Serials $allTargets -Callsigns @($CALLSIGN1, $CALLSIGN2, $CALLSIGN3) -Slots @(0, 1, 2) -Phase 'admission recheck failure' -Baseline @{} | Out-Null
        throw 'Expected native consent changed before approval'
    }
    $runId = [guid]::NewGuid().ToString('N')
    Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', $scriptName, '--es', 'run_id', $runId) -Seconds 5 | Out-Null
    if (-not (Wait-ForCondition -Description 'Host F6 approval producer completed' -TimeoutSec 10 -PollMs 250 -Condition {
                $json = Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'run-as', $PACKAGE, 'cat', 'files/automation_result.json') -Seconds 3
                try { $result = $json | ConvertFrom-Json } catch { return $false }
                return (Test-AutomationResultRunId -Result $result -ExpectedRunId $runId) -and (Test-AutomationResultComplete -Result $result)
            })) { throw 'Host approval action did not complete' }
    Write-Status 'ONLINE_EVIDENCE expected authenticated fixture received explicit native F6 approval' 'Green'
}

function Set-OnlineFault {
    param([string]$Ws = 'pass', [string]$Udp = 'pass')
    $next = "$faultControl.next"
    @{ ws = $Ws; udp = $Udp } | ConvertTo-Json -Compress | Set-Content -LiteralPath $next -Encoding utf8NoBOM
    [IO.File]::Move($next, $faultControl, $true)
    Start-Sleep -Milliseconds 150
    if ($faultProcess -and $faultProcess.HasExited) { throw 'Owned network fault bridge exited' }
}

function Assert-OnlineClean {
    param([string[]]$Serials = $allTargets, [switch]$NoDisconnect)
    if (-not $NoDisconnect) {
        foreach ($serial in $Serials) { Send-MpCommand -Serial $serial -Command 'disconnect' }
    }
    if (-not (Wait-ForCondition -Description 'Server resource baseline' -TimeoutSec 20 -PollMs 250 -Condition {
                $status = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2
                return $status.rooms -eq 0 -and $status.relay_sessions -eq 0 -and $status.pending_and_authenticated_sockets -eq 0
            })) { throw 'Server resources remained after disconnect' }
    foreach ($serial in $Serials) {
        if (-not (Wait-ForCondition -Description 'Client resource baseline' -TimeoutSec 15 -PollMs 500 -Condition {
                    $control = Get-MpIntrospection -Serial $serial
                    if (-not $control) { return $false }
                    $r = $control.online_resources
                    return $control.status -eq 'DISCONNECTED' -and $r.proxy_workers -eq 0 -and $r.background_jobs -eq 0 -and -not $r.candidate_socket -and -not $r.upnp_mapping -and -not $r.game_state_updates
                })) { throw 'Client resources remained after disconnect' }
    }
}

function Start-OnlineRecoveryGame {
    Assert-OnlineClean
    foreach ($serial in @($EMU1, $EMU2)) {
        Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) -Seconds 10 | Out-Null
        if (-not (Start-SetupActivity -Serial $serial)) { throw 'Recovery launcher failed' }
        $callsign = if ($serial -eq $EMU1) { $CALLSIGN1 } else { $CALLSIGN2 }
        Send-MpCommand -Serial $serial -Command 'set_callsign' -Extras @('--es', 'callsign', $callsign)
        Send-MpCommand -Serial $serial -Command 'connect' -Extras @('--es', 'url', $testServerUrl)
        if (-not (Wait-ForCondition -Description 'Recovery authentication' -TimeoutSec 50 -PollMs 500 -Condition {
                    $control = Get-MpIntrospection -Serial $serial
                    return $control -and $control.status -eq 'CONNECTED'
                })) { throw 'Recovery authentication failed' }
    }
    Send-MpCommand -Serial $EMU1 -Command 'create_lobby' -Extras @('--es', 'game', $Game, '--es', 'mission', $MISSION, '--es', 'mode', $MODE, '--ei', 'max_players', '2')
    if (-not (Wait-ForCondition -Description 'Recovery host room' -TimeoutSec 15 -PollMs 500 -Condition {
                $control = Get-MpIntrospection -Serial $EMU1
                return $control -and $control.PSObject.Properties['lobby'] -and $control.lobby.is_host
            })) { throw 'Recovery room creation failed' }
    Send-MpCommand -Serial $EMU2 -Command 'refresh_lobbies'
    if (-not (Wait-ForCondition -Description 'Recovery room visible' -TimeoutSec 15 -PollMs 500 -Condition {
                $control = Get-MpIntrospection -Serial $EMU2
                return $control -and $control.lobby_count -eq 1
            })) { throw 'Recovery room not visible' }
    Send-MpCommand -Serial $EMU2 -Command 'join_first_lobby'
    if (-not (Wait-ForCondition -Description 'Recovery membership' -TimeoutSec 15 -PollMs 500 -Condition {
                $control = Get-MpIntrospection -Serial $EMU2
                return $control -and $control.PSObject.Properties['lobby'] -and $control.lobby.player_count -eq 2
            })) { throw 'Recovery join failed' }
    if ($PublicDirect) { Wait-OnlineDirectReadiness }
    if ($TransportMode -eq 'InitialFallback') { Wait-OnlineInitialFallback }
    foreach ($serial in @($EMU1, $EMU2)) { Send-MpCommand -Serial $serial -Command 'set_ready' -Extras @('--es', 'ready', 'true') }
    if (-not (Wait-ForCondition -Description 'Recovery ready roster' -TimeoutSec 15 -PollMs 500 -Condition {
                return Test-AllLobbyPlayersReady -Serial $EMU1 -ExpectedPlayers 2 -RequireDirect:$PublicDirect
            })) { throw 'Recovery ready failed' }
    Send-MpCommand -Serial $EMU1 -Command 'start_game'
    foreach ($serial in @($EMU1, $EMU2)) {
        if (-not (Wait-ForCondition -Description 'Recovery launch assignment' -TimeoutSec 30 -PollMs 500 -Condition {
                    $control = Get-MpIntrospection -Serial $serial
                    return $control -and $control.game_launch_pending
                })) { throw 'Recovery launch assignment missing' }
        Send-MpCommand -Serial $serial -Command 'launch_game'
    }
    Assert-OnlinePeerMatrix -Serials @($EMU1, $EMU2) -Callsigns @($CALLSIGN1, $CALLSIGN2) -Slots @(0, 1) -Phase 'explicit fresh-game recovery'
}

function Test-OnlineOutageTerminal {
    param([string]$Serial)
    $control = Get-MpIntrospection -Serial $Serial
    $device = 'device'
    if (-not $control) {
        $device = (Adb-Dev-Timeout -Serial $Serial -AdbArgs @('get-state') -Seconds 2 | Out-String).Trim()
        if ($device -notin @('device', 'offline', 'unauthorized')) { $device = 'unavailable' }
    }
    $hasError = $control -and $control.PSObject.Properties['error'] -and [bool]$control.error
    $hasRoom = $control -and [bool]$control.PSObject.Properties['lobby']
    $workers = -1
    if ($control -and $control.PSObject.Properties['online_resources'] -and
        $control.online_resources.PSObject.Properties['proxy_workers']) {
        $workers = [int]$control.online_resources.proxy_workers
    }
    $terminal = [bool]($control -and $hasError -and -not $hasRoom -and $workers -eq 0)
    Write-Status "ONLINE_OUTAGE client=$Serial adb=$device control=$([bool]$control) error=$([bool]$hasError) room=$([bool]$hasRoom) proxy_workers=$workers terminal=$terminal" 'Gray'
    return $terminal
}

function Test-OnlineOutageHostSurvival {
    param($Control, $Native, [string]$Room, [string]$Player, [long]$Attempt, [string]$Callsign)
    if (-not $Control -or -not $Native -or $Attempt -le 0) { return $false }
    foreach ($field in @('lobby', 'online_resources', 'player_id')) {
        if (-not $Control.PSObject.Properties[$field]) { return $false }
    }
    if ($Control.PSObject.Properties['error'] -and $Control.error) { return $false }
    $lobby = $Control.lobby; $resources = $Control.online_resources
    if (-not $lobby -or -not $resources) { return $false }
    foreach ($field in @('lobby_id', 'is_host', 'players')) {
        if (-not $lobby.PSObject.Properties[$field]) { return $false }
    }
    foreach ($field in @('runtime_attempt_epoch', 'peers', 'proxy_workers')) {
        if (-not $resources.PSObject.Properties[$field]) { return $false }
    }
    $players = @($lobby.players)
    if ($lobby.lobby_id -cne $Room -or -not $lobby.is_host -or $Control.player_id -cne $Player -or
        $players.Count -ne 1 -or $players[0].player_id -cne $Player -or $players[0].slot -ne 0 -or
        $resources.runtime_attempt_epoch -ne $Attempt -or @($resources.peers).Count -ne 0 -or
        $resources.proxy_workers -lt 0 -or $resources.proxy_workers -gt 1) { return $false }
    foreach ($field in @('in_game', 'is_network', 'game_window_is_front', 'multiplayer')) {
        if (-not $Native.PSObject.Properties[$field]) { return $false }
    }
    $mp = $Native.multiplayer
    if (-not $mp) { return $false }
    foreach ($field in @('i_am_master', 'my_player_num', 'players', 'join_request_pending')) {
        if (-not $mp.PSObject.Properties[$field]) { return $false }
    }
    $connected = @($mp.players | Where-Object { $_.connected -eq 1 })
    return $Native.in_game -and $Native.is_network -and $Native.game_window_is_front -and
    $mp.i_am_master -and $mp.my_player_num -eq 0 -and -not $mp.join_request_pending -and
    $connected.Count -eq 1 -and $connected[0].callsign -ieq $Callsign
}

function Assert-OnlineUdpOutage {
    $before = Get-MpIntrospection -Serial $EMU1
    if (-not $before -or -not $before.PSObject.Properties['lobby'] -or
        $before.online_resources.runtime_attempt_epoch -le 0) { throw 'UDP outage requires a live owned host attempt' }
    $room = $before.lobby.lobby_id; $player = $before.player_id; $attempt = $before.online_resources.runtime_attempt_epoch
    Set-OnlineFault -Udp 'drop'
    $bounded = Wait-ForCondition -Description 'UDP outage visible terminal or live host-only state' -TimeoutSec 25 -PollMs 500 -Condition {
        $joinerStopped = Test-OnlineOutageTerminal -Serial $EMU2
        $hostStopped = Test-OnlineOutageTerminal -Serial $EMU1
        if (-not $joinerStopped) { return $false }
        try { $status = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2 } catch { return $false }
        if ($status.relay_sessions -ne 0 -or $status.pending_native_admissions -ne 0 -or
            $status.pending_and_authenticated_sockets -ne 2) { return $false }
        if ($hostStopped) { return $status.rooms -eq 0 }
        $hostControl = Get-MpIntrospection -Serial $EMU1
        $native = Get-GameIntrospection -Serial $EMU1 -Fresh
        $survived = Test-OnlineOutageHostSurvival -Control $hostControl -Native $native -Room $room -Player $player -Attempt $attempt -Callsign $CALLSIGN1
        Write-Status "ONLINE_OUTAGE client=$EMU1 live_host_only=$survived rooms=$($status.rooms) relay_sessions=$($status.relay_sessions)" 'Gray'
        return $survived -and $status.rooms -eq 1
    }
    if (-not $bounded) {
        Write-OnlineOutageDiagnostics
        throw 'Long UDP loss did not produce bounded visible cleanup or a verified live host-only room'
    }
}

function Write-OnlineOutageDiagnostics {
    foreach ($target in @($EMU1, $EMU2)) {
        # Capture before Cleanup force-stops the applications; diagnostic failure cannot qualify the gate
        try { Test-OnlineOutageTerminal -Serial $target | Out-Null } catch { Write-Status "ONLINE_OUTAGE client=$target control_probe=unavailable" 'Yellow' }
        try {
            $processes = @(Get-ManagedEmulatorProcesses -Serial $target)
            Write-Status "ONLINE_OUTAGE client=$target emulator_processes=$($processes.Count)" 'Yellow'
            $logs = Get-ManagedEmulatorLaunchLogPaths -AvdName $AVD_MAP[$target]
            foreach ($stream in @('Stdout', 'Stderr')) {
                if (Test-Path -LiteralPath $logs[$stream] -PathType Leaf) {
                    Write-Status "ONLINE_OUTAGE client=$target emulator_log=$stream tail_begin" 'Yellow'
                    Get-Content -LiteralPath $logs[$stream] -Tail 40 | ForEach-Object {
                        $line = [string]$_
                        Write-Status $line.Substring(0, [Math]::Min(500, $line.Length)) 'Gray'
                    }
                }
            }
        } catch { Write-Status "ONLINE_OUTAGE client=$target process_diagnostics=unavailable" 'Yellow' }
    }
}

function Invoke-OnlineReturnToGame {
    $gamePid = (Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'pidof', "${PACKAGE}:game") -Seconds 10).Trim()
    $controlPid = (Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'pidof', $PACKAGE) -Seconds 10).Trim()
    $before = Get-MpIntrospection -Serial $EMU1
    if ($gamePid -notmatch '^\d+$' -or $controlPid -notmatch '^\d+$' -or -not $before.lobby -or $before.online_resources.runtime_attempt_epoch -le 0) {
        throw 'Return to Game requires a live owned launch'
    }
    $room = $before.lobby.lobby_id
    $attempt = $before.online_resources.runtime_attempt_epoch
    # Start-SetupActivity intentionally force-stops; this transition must preserve both processes
    Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'am', 'start', '-n', "$PACKAGE/$ACTIVITY") -Seconds 10 | Out-Null
    if (-not (Wait-SetupReady -Serial $EMU1 -TimeoutSec 15)) { throw 'Host launcher return setup failed' }
    Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '--es', 'command', 'launch') -Seconds 10 | Out-Null
    if (-not (Wait-ForCondition -Description 'Host Return to Game preserves launch ownership' -TimeoutSec 15 -PollMs 500 -Condition {
                $native = Get-GameIntrospection -Serial $EMU1 -Fresh
                $control = Get-MpIntrospection -Serial $EMU1
                $currentGamePid = (Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'pidof', "${PACKAGE}:game") -Seconds 10).Trim()
                $currentControlPid = (Adb-Dev-Timeout -Serial $EMU1 -AdbArgs @('shell', 'pidof', $PACKAGE) -Seconds 10).Trim()
                if ($currentGamePid -ne $gamePid -or $currentControlPid -ne $controlPid) { throw 'Return to Game restarted an owned process' }
                return $native -and $native.in_game -and $native.is_network -and $native.multiplayer.my_player_num -eq 0 -and
                $control -and $control.lobby.lobby_id -eq $room -and $control.online_resources.runtime_attempt_epoch -eq $attempt
            })) { throw 'Return to Game changed the room or runtime attempt' }
    Assert-OnlinePeerMatrix -Serials @($EMU1, $EMU2) -Callsigns @($CALLSIGN1, $CALLSIGN2) -Slots @(0, 1) -Phase 'Return to Game preserves original pair'
    Write-Status 'ONLINE_EVIDENCE Return to Game preserved native/control processes, room, attempt epoch and original traffic' 'Green'
}

function Invoke-OnlineBackgroundResume {
    foreach ($hidden in @($EMU1, $EMU2)) {
        $pidBefore = (Adb-Dev -Serial $hidden -AdbArgs @('shell', 'pidof', "${PACKAGE}:game")).Trim()
        if ($pidBefore -notmatch '^\d+$') { throw 'Missing background game process' }
        Adb-Dev -Serial $hidden -AdbArgs @('shell', 'am', 'start', '-a', 'android.settings.SETTINGS') | Out-Null
        if (-not (Wait-ForCondition -Description 'Online background visibility' -TimeoutSec 20 -PollMs 1000 -Condition {
                    $intro = Get-GameIntrospection -Serial $hidden -Fresh
                    return $intro -and $intro.android_lifecycle.observed_visibility -eq 'background'
                })) { throw 'Online client did not background' }
        # Do not promote the hidden process through introspection broadcasts during the dwell
        Start-Sleep -Seconds 60
        Adb-Dev -Serial $hidden -AdbArgs @('shell', 'am', 'start', '-a', 'android.intent.action.DIAL') | Out-Null
        Start-Sleep -Seconds 50
        Adb-Dev -Serial $hidden -AdbArgs @('shell', 'monkey', '-p', $PACKAGE, '-c', 'android.intent.category.LAUNCHER', '1') | Out-Null
        if (-not (Wait-SetupReady -Serial $hidden -TimeoutSec 30)) { throw 'Resume launcher unavailable' }
        if (-not (Wait-ForCondition -Description 'Resume launcher focus' -TimeoutSec 20 -PollMs 500 -Condition {
                    $windows = Adb-Dev -Serial $hidden -AdbArgs @('shell', 'dumpsys', 'window')
                    return $windows -match "mCurrentFocus=.*$([regex]::Escape($PACKAGE))/.*SetupActivity"
                })) { throw 'Resume launcher did not receive focus' }
        Adb-Dev -Serial $hidden -AdbArgs @('shell', 'input', 'keyevent', 'KEYCODE_BACK') | Out-Null
        if (-not (Wait-ForCondition -Description 'Online foreground visibility' -TimeoutSec 20 -PollMs 1000 -Condition {
                    $intro = Get-GameIntrospection -Serial $hidden -Fresh
                    return $intro -and $intro.android_lifecycle.observed_visibility -eq 'foreground'
                })) { throw 'Online client did not resume' }
        $pidAfter = (Adb-Dev -Serial $hidden -AdbArgs @('shell', 'pidof', "${PACKAGE}:game")).Trim()
        if ($pidAfter -ne $pidBefore) { throw 'Background/resume restarted the game process' }
        Assert-OnlinePeerMatrix -Serials @($EMU1, $EMU2) -Callsigns @($CALLSIGN1, $CALLSIGN2) -Slots @(0, 1) -Phase "background/resume $hidden"
    }
}

function Cleanup {
    Write-Status "Cleaning up..."
    try { Remove-OnlineProbeFaults } catch { $script:testPassed = $false; Write-Status $_.Exception.Message 'Red' }
    try { if ($logcatProc1 -and -not $logcatProc1.HasExited) { Stop-Process -Id $logcatProc1.Id -Force } } catch {}
    try { if ($logcatProc2 -and -not $logcatProc2.HasExited) { Stop-Process -Id $logcatProc2.Id -Force } } catch {}
    # Force-stop the app on both emulators BEFORE killing the server.
    # Otherwise the relay dies first, the game detects a timeout ~15s later,
    # and shows "Host left the game!" dialogs.
    foreach ($emu in $allTargets) {
        try {
            & $ADB -s $emu shell am force-stop $PACKAGE 2>&1 | Out-Null
        } catch {}
    }
    if ($script:faultProcess -and -not $script:faultProcess.HasExited) {
        $script:faultProcess.Kill()
        if (-not $script:faultProcess.WaitForExit(5000)) { throw 'Owned fault bridge did not stop' }
    }
    if ($script:serverProcess -and -not $script:serverProcess.HasExited) {
        Write-Status "Stopping matchmaking server (PID $($script:serverProcess.Id))..."
        try { $script:serverProcess.Kill() } catch {}
        try { $script:serverProcess.WaitForExit(5000) } catch {}
    }
    # Wait briefly for log file to flush
    Start-Sleep -Milliseconds 500
    # Print server log tail for diagnostics
    $serverLogDir = Join-Path $REPO_ROOT "temp"
    $serverLogFile = Get-ChildItem $serverLogDir -Filter "dxx-matchmaking.log*" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($serverLogFile) {
        $lines = Get-Content $serverLogFile.FullName | Select-Object -Last 30
        if ($lines) {
            Write-Status "  Server log ($($serverLogFile.Name), last 30 lines):" "Gray"
            foreach ($l in $lines) {
                if ($l.Trim()) { Write-Status "    $($l.Trim())" "Gray" }
            }
        }
    }
}

# ── Main Test Flow ──────────────────────────────────────────────────────────

try {

    # -- Step 0: Verify both emulators are online --
    Write-Status "=== Two-Player Multiplayer Integration Test ===" "White"
    Write-Status "Game: $Game | Mission: $MISSION | Mode: $MODE"
    Write-Status ""

    # -- Step 0: Ensure both emulators are online (auto-start if needed) --
    Write-Status "Checking emulators..."
    Start-EmulatorIfNeeded -Serial $EMU1 -AvdMap $AVD_MAP
    Start-EmulatorIfNeeded -Serial $EMU2 -AvdMap $AVD_MAP
    Write-Status "Both emulators online: $EMU1, $EMU2" "Green"

    if ($PublicShareware) {
        # Validate ALL disposable targets before resetting any app state
        foreach ($emu in $allTargets) {
            if ($emu -notmatch '^emulator-[0-9]+$' -or
                (Adb-Dev-Timeout -Serial $emu -AdbArgs @('shell', 'getprop', 'ro.kernel.qemu') -Seconds 5) -ne '1') {
                throw "Public online fixture requires disposable emulator targets"
            }
        }
        foreach ($emu in $allTargets) {
            $cleared = Adb-Dev-Timeout -Serial $emu -AdbArgs @('shell', 'pm', 'clear', $PACKAGE) -Seconds 30
            if ($cleared -ne 'Success') { throw "Could not reset disposable emulator app state" }
            Adb-Dev-Timeout -Serial $emu -AdbArgs @('shell', 'run-as', $PACKAGE, 'mkdir', '-p', $script:DEFAULT_SET_DIR) -Seconds 10 | Out-Null
            foreach ($file in $publicConfig.RequiredHashes) {
                $staged = "/data/local/tmp/online-public-$($file.File)"
                $destination = "$($script:DEFAULT_SET_DIR)/$($file.File)"
                try {
                    Adb-Dev-Timeout -Serial $emu -AdbArgs @('push', (Join-Path $publicDirectory $file.File), $staged) -Seconds 30 | Out-Null
                    Adb-Dev-Timeout -Serial $emu -AdbArgs @('shell', 'run-as', $PACKAGE, 'cp', $staged, $destination) -Seconds 10 | Out-Null
                    $actual = Adb-Dev-Timeout -Serial $emu -AdbArgs @('shell', 'run-as', $PACKAGE, 'sha256sum', $destination) -Seconds 10
                    if ($actual -cne "$($file.Sha256)  $destination") { throw "Public asset hash mismatch: $($file.File)" }
                } finally {
                    Adb-Dev-Timeout -Serial $emu -AdbArgs @('shell', 'rm', '-f', $staged) -Seconds 5 | Out-Null
                }
            }
        }
    } else {
        foreach ($emu in @($EMU1, $EMU2)) {
            if (-not (Ensure-StandardGameDataOnDevice -Serial $emu)) {
                throw "Could not ensure standard game data on $emu"
            }
        }
    }
    Write-Status "Game data verified on both emulators" "Green"

    # Clear state inherited from earlier emulator tests
    foreach ($emu in $allTargets) {
        Reset-DeviceGameState -Serial $emu
    }

    # -- Step 1: Start matchmaking server --
    Write-Status ""
    Write-Status "--- Phase 1: Start matchmaking server ---" "White"

    # Pre-create firewall rules to prevent UAC prompts during test
    if (-not $PublicShareware) { Ensure-FirewallRules }

    # Kill any existing server on the ports we need (9000, 8080, 9001).
    # run_all_tests.ps1 may have started its own server for the tier.
    $serverPorts = @(9000, 8080, 9001)
    if ($PublicShareware) {
        foreach ($port in @(9000, 8080)) {
            $probe = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $port)
            try { $probe.Start() } finally { $probe.Stop() }
        }
        # Occupied owned-test ports fail admission; never kill unrelated processes
    } else {
        Stop-RegressionProcessesListeningOnPorts -Ports $serverPorts -WaitSeconds 10
    }

    Write-Status "Starting matchmaking server..."
    $serverDir = Join-Path $REPO_ROOT "server"
    $serverBin = Resolve-RegressionBuildTool -Directory (Join-RegressionPath $serverDir "target" "release") -BaseName "dxx-matchmaking"

    # Build the matching locked server unless the caller explicitly skips it
    if (-not $serverBin -or -not $SkipBuild) {
        Write-Status "Building matching server..."
        $serverManifest = Join-Path $serverDir "Cargo.toml"
        $buildOut = & cargo build --locked --release --manifest-path $serverManifest 2>&1 | Out-String
        $serverBuildExit = $LASTEXITCODE
        $serverBin = Resolve-RegressionBuildTool -Directory (Join-RegressionPath $serverDir "target" "release") -BaseName "dxx-matchmaking"
        if ($serverBuildExit -ne 0 -or -not $serverBin) {
            Write-Status "FAIL: Server build failed" "Red"
            Write-Status $buildOut "Yellow"
            exit 1
        }
    }

    # Set env vars for server, then launch with Start-Process for clean stderr capture
    # Installation/PoW authentication is only accepted by loopback development servers
    $env:SKIP_GPGS_VERIFY = "true"
    $env:INSTALLATION_AUTH_AUDIENCE = if ($Tls) { "https://10.0.2.2:9000" } else { "http://10.0.2.2:9000" }
    $env:RUST_LOG = "info"
    # The public campaign owns a synthetic database and loopback-only listeners
    if ($PublicShareware) {
        $env:DB_PATH = ':memory:'
        $env:STUN_LISTEN_ADDR = '127.0.0.1:3478'
        $env:STUN_LISTEN_ADDR_ALT = '127.0.0.1:3479'
        $env:STUN_PUBLIC_ADDRS = '10.0.2.2:3478,10.0.2.2:3479'
        $env:ADMIN_HTTP_LISTEN_ADDR = ''
    }
    # Bind to localhost to avoid Windows Firewall prompts (emulator
    # reaches host via 10.0.2.2 which maps to loopback)
    $env:WS_LISTEN_ADDR = if ($PublicShareware) { "127.0.0.1:9002" } else { "127.0.0.1:9000" }
    $env:HTTP_LISTEN_ADDR = "127.0.0.1:8080"
    # Configure built-in UDP relay so LocalhostProxy can route game traffic.
    # Emulators reach host loopback at 10.0.2.2.
    $env:RELAY_LISTEN_ADDR = if ($PublicShareware) { "127.0.0.1:9003" } else { "127.0.0.1:9001" }
    $env:RELAY_PUBLIC_ADDR = "10.0.2.2:9001"
    # Relay campaigns force relay; direct campaigns must prove current peer reachability
    $env:FORCE_RELAY = if ($PublicDirect -or $TransportMode -eq 'InitialFallback') { "false" } else { "true" }
    $env:LOG_DIR = (Join-Path $REPO_ROOT "temp")
    if ($Tls) {
        $tlsDir = Join-Path $REPO_ROOT "temp\tls"
        New-Item -Path $tlsDir -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null
        $tlsCert = Join-Path $tlsDir "server.crt"
        $tlsKey = Join-Path $tlsDir "server.key"
        if (-not (Test-Path $tlsCert) -or -not (Test-Path $tlsKey)) {
            Write-Status "  Generating self-signed TLS certificate..."
            $opensslExe = Resolve-RegressionOpenSslTool -DepBase $DEP_BASE
            & $opensslExe req -x509 -newkey rsa:2048 -keyout $tlsKey -out $tlsCert `
                -days 365 -nodes -subj "/CN=localhost" 2>&1 | Out-Null
        }
        if ((Test-Path $tlsCert) -and (Test-Path $tlsKey)) {
            $env:TLS_CERT_PATH = $tlsCert
            $env:TLS_KEY_PATH = $tlsKey
            Write-Status "  TLS enabled"
        } else {
            Write-Status "  WARNING: TLS cert generation failed, falling back to plain WS" "Yellow"
        }
    }
    $script:serverLogPath = Join-Path $REPO_ROOT "temp\matchmaking_server.log"
    $serverProcess = Start-Process -FilePath $serverBin -WorkingDirectory $serverDir `
        -NoNewWindow -PassThru `
        -RedirectStandardError $script:serverLogPath `
        -RedirectStandardOutput (Join-Path $REPO_ROOT "temp\server_stdout.log")
    Write-Status "Server starting (PID $($serverProcess.Id))..."

    # Readiness includes the required UDP listeners, not just an open WS port
    $serverReady = Wait-ForCondition -Description "Matchmaking listener readiness" -TimeoutSec 15 -PollMs 1000 -Condition {
        if ($serverProcess.HasExited) { throw "Matchmaking server exited before readiness" }
        try {
            $ready = Invoke-RestMethod -Uri "http://127.0.0.1:8080/api/v1/ready" -TimeoutSec 2
            return ($ready.ready -eq $true)
        } catch { return $false }
    }
    if (-not $serverReady) {
        Write-Status "FAIL: Server didn't start" "Red"
        Cleanup
        exit 1
    }
    Write-Status "Matchmaking server is running" "Green"

    # -- Step 2: Launch SetupActivity on both emulators --
    Write-Status ""
    Write-Status "--- Phase 2: Launch SetupActivity on both emulators ---" "White"

    if (-not (Start-SetupActivity -Serial $EMU1)) {
        Write-Status "FAIL: SetupActivity didn't start on $EMU1" "Red"
        Cleanup; exit 1
    }
    if (-not (Start-SetupActivity -Serial $EMU2)) {
        Write-Status "FAIL: SetupActivity didn't start on $EMU2" "Red"
        Cleanup; exit 1
    }
    Write-Status "SetupActivity ready on both emulators" "Green"

    # Reset-GameState preserves pilots, including a prior test's CD music choice.
    # This fixture has no CD source; set music through the engine preference API.
    foreach ($emu in @($EMU1, $EMU2)) {
        $musicResult = Adb-Dev-Timeout -Serial $emu -AdbArgs @(
            'shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND',
            '--es', 'command', 'write_music_prefs', '--es', 'source', 'midi'
        ) -Seconds 10
        if (-not $musicResult -or $musicResult -notmatch 'result=0') {
            throw "Could not initialize MIDI preferences on ${emu}: $musicResult"
        }
    }

    # -- Step 3: Connect to matchmaking --
    Write-Status ""
    if ($TransportMode -eq 'InitialFallback') {
        foreach ($serial in $allTargets) { Install-OnlineProbeFault -Serial $serial }
    }
    Write-Status "--- Phase 3: Connect to matchmaking ---" "White"

    # Drive the multiplayer flow through the SetupActivity MP_COMMAND receiver.
    # This avoids coupling the test to launcher button exposure details.
    Send-MpCommand -Serial $EMU1 -Command "set_callsign" -Extras @("--es", "callsign", $CALLSIGN1)
    Send-MpCommand -Serial $EMU2 -Command "set_callsign" -Extras @("--es", "callsign", $CALLSIGN2)
    Start-Sleep -Milliseconds 500

    if ($PublicShareware) {
        Set-OnlineFault
        $faultScript = Join-Path $serverDir 'loopback_fault_proxy.py'
        $faultProcess = Start-Process -FilePath 'python3' -ArgumentList @($faultScript, '--control', $faultControl) -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $REPO_ROOT 'temp/online-fault.log') -RedirectStandardError (Join-Path $REPO_ROOT 'temp/online-fault-error.log')
        Start-Sleep -Seconds 1
        if ($faultProcess.HasExited) { throw 'Owned fault bridge failed to start' }
    }
    $testServerUrl = if ($Tls) { "wss://10.0.2.2:9000/ws" } else { "ws://10.0.2.2:9000/ws" }
    if ($PublicShareware) {
        Measure-InstallationWork -ServerUrl $testServerUrl
        Send-MpCommand -Serial $EMU1 -Command 'set_callsign' -Extras @('--es', 'callsign', $CALLSIGN1)
        Send-MpCommand -Serial $EMU2 -Command 'set_callsign' -Extras @('--es', 'callsign', $CALLSIGN2)
    }
    Write-Status "Connecting via $testServerUrl"
    Send-MpCommand -Serial $EMU1 -Command "connect" -Extras @("--es", "url", $testServerUrl)
    Send-MpCommand -Serial $EMU2 -Command "connect" -Extras @("--es", "url", $testServerUrl)

    $conn1 = Wait-ForCondition -Description "Player 1 connected" -TimeoutSec 50 -PollMs 750 -Condition {
        $mp = Get-MpIntrospection -Serial $EMU1
        return ($mp -and $mp.status -eq "CONNECTED")
    }
    if (-not $conn1) {
        Write-Status "FAIL: Player 1 didn't connect" "Red"
        $mp1 = Get-MpIntrospection -Serial $EMU1
        if ($mp1) { Write-Status "  Status: $($mp1.status), Error: $($mp1.error)" "Yellow" }
        Cleanup; exit 1
    }
    $conn2 = Wait-ForCondition -Description "Player 2 connected" -TimeoutSec 50 -PollMs 750 -Condition {
        $mp = Get-MpIntrospection -Serial $EMU2
        return ($mp -and $mp.status -eq "CONNECTED")
    }
    if (-not $conn2) {
        Write-Status "FAIL: Player 2 didn't connect" "Red"
        Cleanup; exit 1
    }
    Write-Status "Both players connected" "Green"

    # -- Step 4: Player 1 creates a lobby --
    Write-Status ""
    Write-Status "--- Phase 4: Player 1 creates a lobby ---" "White"

    # Use MP_COMMAND for create_lobby (the dialog with dropdowns is complex to tap through)
    Send-MpCommand -Serial $EMU1 -Command "create_lobby" -Extras @(
        "--es", "game", $Game,
        "--es", "mission", $MISSION,
        "--es", "mode", $MODE,
        "--ei", "max_players", $(if ($PublicShareware) { "3" } else { "2" })
    )

    $lobbyCreated = Wait-ForCondition -Description "Player 1 in lobby" -TimeoutSec 10 -PollMs 750 -Condition {
        $mp = Get-MpIntrospection -Serial $EMU1
        return ($mp -and $mp.PSObject.Properties['lobby'] -and $mp.lobby -and $mp.lobby.is_host -eq $true)
    }
    if (-not $lobbyCreated) {
        Write-Status "FAIL: Lobby not created" "Red"
        Cleanup; exit 1
    }
    Write-Status "Lobby created by $CALLSIGN1" "Green"

    # -- Step 5: Player 2 joins the lobby --
    Write-Status ""
    Write-Status "--- Phase 5: Player 2 joins the lobby ---" "White"

    Send-MpCommand -Serial $EMU2 -Command "refresh_lobbies"
    $lobbiesVisible = Wait-ForCondition -Description "Player 2 sees lobby" -TimeoutSec 10 -PollMs 750 -Condition {
        $mp = Get-MpIntrospection -Serial $EMU2
        return ($mp -and $mp.lobby_count -gt 0)
    }
    if (-not $lobbiesVisible) {
        Write-Status "FAIL: Player 2 can't see any lobbies" "Red"
        Cleanup; exit 1
    }

    # Join via MP_COMMAND -- the "Join" button is inside a LazyColumn which
    # Compose's accessibility provider doesn't expose to sequential ID scans.
    Send-MpCommand -Serial $EMU2 -Command "join_first_lobby"

    $joined = Wait-ForCondition -Description "Both players in lobby" -TimeoutSec 10 -PollMs 750 -Condition {
        $mp = Get-MpIntrospection -Serial $EMU2
        return ($mp -and $mp.PSObject.Properties['lobby'] -and $mp.lobby -and $mp.lobby.player_count -eq 2)
    }
    if (-not $joined) {
        Write-Status "FAIL: Player 2 didn't join lobby" "Red"
        Cleanup; exit 1
    }
    Write-Status "Both players in lobby" "Green"

    # -- Step 6: Quick chat verification (non-fatal) --
    Write-Status ""
    Write-Status "--- Phase 6: Chat verification ---" "White"

    Send-MpCommand -Serial $EMU1 -Command "chat" -Extras @("--es", "text", "Hello_from_Host")
    Start-Sleep -Milliseconds 500
    Send-MpCommand -Serial $EMU2 -Command "chat" -Extras @("--es", "text", "Hello_from_Joiner")

    $chatOk = Wait-ForCondition -Description "Chat delivered" -TimeoutSec 10 -PollMs 1500 -Condition {
        $m1 = Get-MpIntrospection -Serial $EMU1
        $m2 = Get-MpIntrospection -Serial $EMU2
        $p1HasJoiner = $m1 -and $m1.chat -and ($m1.chat | Where-Object { $_.text -eq "Hello_from_Joiner" })
        $p2HasHost = $m2 -and $m2.chat -and ($m2.chat | Where-Object { $_.text -eq "Hello_from_Host" })
        return ($p1HasJoiner -and $p2HasHost)
    }
    if (-not $chatOk) {
        Write-Status "WARN: Chat not fully delivered (non-fatal)" "Yellow"
    } else {
        Write-Status "Chat works both ways" "Green"
    }

    # -- Step 7: Ready up and start game --
    Write-Status ""
    Write-Status "--- Phase 7: Ready up and start game ---" "White"

    # Probe reports precede Ready on each WebSocket; the ready broadcast then acknowledges server receipt
    if ($PublicDirect) { Wait-OnlineDirectReadiness }
    if ($TransportMode -eq 'InitialFallback') { Wait-OnlineInitialFallback }
    Write-Status "Setting both players ready via MP_COMMAND"
    Send-MpCommand -Serial $EMU1 -Command "set_ready" -Extras @("--es", "ready", "true")
    Send-MpCommand -Serial $EMU2 -Command "set_ready" -Extras @("--es", "ready", "true")

    $bothReady = Wait-ForCondition -Description "Both players ready" -TimeoutSec 10 -PollMs 750 -Condition {
        return Test-AllLobbyPlayersReady -Serial $EMU1 -ExpectedPlayers 2 -RequireDirect:$PublicDirect
    }
    if (-not $bothReady) {
        $mp = Get-MpIntrospection -Serial $EMU1
        if ($mp -and $mp.PSObject.Properties['lobby'] -and $mp.lobby -and $mp.lobby.PSObject.Properties['players']) {
            $readySummary = @($mp.lobby.players) | ForEach-Object {
                "$($_.callsign)=$($_.ready)"
            }
            if ($readySummary) {
                Write-Status "  Ready state: $($readySummary -join ', ')" "Yellow"
            }
        }
        Write-Status "FAIL: Not all players ready" "Red"
        Cleanup; exit 1
    }
    Write-Status "Both players ready" "Green"

    # Clear stale introspect files so Phase 8 doesn't read old game state
    foreach ($emu in @($EMU1, $EMU2)) {
        Adb-Dev -Serial $emu -AdbArgs @(
            "shell", "run-as", $PACKAGE, "rm", "-f", "files/introspect.json"
        ) | Out-Null
    }

    # Clear logcat and start capture BEFORE start_game
    & $ADB -s $EMU1 logcat -c 2>&1 | Out-Null
    & $ADB -s $EMU2 logcat -c 2>&1 | Out-Null
    $logcatFile1 = Join-Path $REPO_ROOT "temp\emu1_logcat_phase8.txt"
    $logcatFile2 = Join-Path $REPO_ROOT "temp\emu2_logcat_phase8.txt"
    $logcatProc1 = Start-Process -FilePath $ADB -ArgumentList "-s", $EMU1, "logcat", "-s", "DXX-MP:*", "DXX-Setup:*", "DXX-RouteMetadata:*", "DXX-Redux:*", "dxxredux:*", "MatchmakingService:*", "LocalhostProxy:*", "DEBUG:*", "AndroidRuntime:*", "libc:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile1 -RedirectStandardError (Join-Path $REPO_ROOT "temp\emu1_logcat_err.txt")
    $logcatProc2 = Start-Process -FilePath $ADB -ArgumentList "-s", $EMU2, "logcat", "-s", "DXX-MP:*", "DXX-Setup:*", "DXX-RouteMetadata:*", "DXX-Redux:*", "dxxredux:*", "MatchmakingService:*", "LocalhostProxy:*", "DEBUG:*", "AndroidRuntime:*", "libc:*" -PassThru -NoNewWindow -RedirectStandardOutput $logcatFile2 -RedirectStandardError (Join-Path $REPO_ROOT "temp\emu2_logcat_err.txt")

    # Start game via MP_COMMAND -- "Start Game" is on the LobbyScreen which
    # uses LazyColumn; same accessibility limitation as the Join button.
    Write-Status "Player 1 starting game..."
    Send-MpCommand -Serial $EMU1 -Command "start_game"

    # -- Step 8: Wait for both players to enter the game --
    # The test drives matchmaking through MP_COMMANDs, so always send explicit
    # launch_game commands after the lobby transitions to GAME_STARTING.
    Write-Status ""
    Write-Status "--- Phase 8: Wait for game launch ---" "White"

    foreach ($emu in @($EMU1, $EMU2)) {
        $launchReady = Wait-ForCondition -Description "Launch information on $emu" -TimeoutSec 15 -PollMs 500 -Condition {
            $mp = Get-MpIntrospection -Serial $emu
            return ($mp -and $mp.game_launch_pending)
        }
        if (-not $launchReady) { throw "No game launch information received on $emu" }
        Send-MpCommand -Serial $emu -Command "launch_game"
    }

    # Both clients must provide live engine evidence; log text cannot qualify a crash
    $script:p8poll = 0
    $script:p8nullCount = 0
    $inGame1 = Wait-ForCondition -Description "Player 1 in game" -TimeoutSec 120 -PollMs 3000 -Condition {
        $script:p8poll++
        $gi = Get-GameIntrospection -Serial $EMU1
        if ($gi) {
            Write-Status "  [poll $($script:p8poll)] EMU1: screen=$($gi.screen_mode) in_game=$($gi.in_game) game_mode=$($gi.game_mode) level=$($gi.current_level_num)" "Gray"
            $script:p8nullCount = 0
        } else {
            $launch = Get-MpIntrospection -Serial $EMU1
            if ($launch -and $launch.launch_error) { throw "Host launch blocked: $($launch.launch_error)" }
            $script:p8nullCount++
            Write-Status "  [poll $($script:p8poll)] EMU1: introspection returned null (consecutive: $($script:p8nullCount))" "Gray"
        }
        if ($gi -and $gi.in_game -eq $true) { return $true }
        return $false
    }
    if (-not $inGame1) {
        Write-Status "FAIL: Player 1 never entered game" "Red"
        $gi = Get-GameIntrospection -Serial $EMU1
        if ($gi) {
            Write-Status "  screen_mode=$($gi.screen_mode) game_mode=$($gi.game_mode) in_game=$($gi.in_game)" "Yellow"
            if ($gi.menu) { Write-Status "  menu: $($gi.menu | ConvertTo-Json -Compress)" "Yellow" }
        }
        # Capture logcat for diagnostics (broad filter to catch crashes)
        $log1 = & $ADB -s $EMU1 logcat -t 200 -s "DXX-MP:*" "MatchmakingService:*" "dxxredux:*" "MainActivity:*" "DEBUG:*" "AndroidRuntime:*" "libc:*" 2>&1 | Out-String
        Write-Status "  EMU1 logcat:" "Gray"
        foreach ($line in ($log1 -split "`n" | Select-Object -Last 40)) {
            if ($line.Trim()) { Write-Status "    $($line.Trim())" "Gray" }
        }
        # Full logcat dump for crash analysis
        try {
            if ($logcatProc1 -and -not $logcatProc1.HasExited) { Stop-Process -Id $logcatProc1.Id -Force -ErrorAction SilentlyContinue }
            if ($logcatProc2 -and -not $logcatProc2.HasExited) { Stop-Process -Id $logcatProc2.Id -Force -ErrorAction SilentlyContinue }
            Start-Sleep -Milliseconds 500
            Write-Status "  Background logcat saved: temp\emu1_logcat_phase8.txt, temp\emu2_logcat_phase8.txt" "Gray"
        } catch { Write-Status "  Could not stop logcat procs: $_" "Gray" }
        # Show last MPDIAG lines from live capture
        if (Test-Path $logcatFile1) {
            Write-Status "  EMU1 MPDIAG lines:" "Gray"
            foreach ($line in (Select-String -Path $logcatFile1 -Pattern "MPDIAG" -SimpleMatch | Select-Object -Last 20)) {
                Write-Status "    $($line.Line)" "Yellow"
            }
        }
        # Also check console output from game introspection
        if ($gi -and $gi.console) {
            Write-Status "  EMU1 console:" "Gray"
            foreach ($c in ($gi.console.lines | Select-Object -Last 10)) {
                Write-Status "    $($c.text)" "Gray"
            }
        }
        # Also dump EMU2 console for cross-reference
        $gi2 = Get-GameIntrospection -Serial $EMU2
        if ($gi2 -and $gi2.console) {
            Write-Status "  EMU2 console:" "Gray"
            foreach ($c in ($gi2.console.lines | Select-Object -Last 15)) {
                Write-Status "    $($c.text)" "Gray"
            }
        }
        Cleanup; exit 1
    }

    $script:p8poll2 = 0
    $script:p8nullCount2 = 0
    $inGame2 = Wait-ForCondition -Description "Player 2 in game" -TimeoutSec 120 -PollMs 3000 -Condition {
        $script:p8poll2++
        $gi = Get-GameIntrospection -Serial $EMU2
        if ($gi) {
            Write-Status "  [poll $($script:p8poll2)] EMU2: in_game=$($gi.in_game) level=$($gi.current_level_num)" "Gray"
        } else {
            $launch = Get-MpIntrospection -Serial $EMU2
            if ($launch -and $launch.launch_error) { throw "Client launch blocked: $($launch.launch_error)" }
            Write-Status "  [poll $($script:p8poll2)] EMU2: no live introspection" "Gray"
        }
        return ($gi -and $gi.in_game -eq $true)
    }
    if (-not $inGame2) {
        Write-Status "FAIL: Player 2 never entered game" "Red"
        $gi = Get-GameIntrospection -Serial $EMU2
        if ($gi) {
            Write-Status "  screen_mode=$($gi.screen_mode) game_mode=$($gi.game_mode) in_game=$($gi.in_game)" "Yellow"
            if ($gi.menu) { Write-Status "  menu: $($gi.menu | ConvertTo-Json -Compress)" "Yellow" }
        }
        if ($gi -and $gi.console) {
            Write-Status "  EMU2 console:" "Gray"
            foreach ($c in ($gi.console.lines | Select-Object -Last 15)) {
                Write-Status "    $($c.text)" "Gray"
            }
        }
        # Also dump EMU1 console
        $gi1 = Get-GameIntrospection -Serial $EMU1
        if ($gi1 -and $gi1.console) {
            Write-Status "  EMU1 console:" "Gray"
            foreach ($c in ($gi1.console.lines | Select-Object -Last 10)) {
                Write-Status "    $($c.text)" "Gray"
            }
        }
        Cleanup; exit 1
    }
    Write-Status "Both players in game" "Green"
    # Stop background logcat capture
    if ($logcatProc1 -and -not $logcatProc1.HasExited) { Stop-Process -Id $logcatProc1.Id -Force -ErrorAction SilentlyContinue }
    if ($logcatProc2 -and -not $logcatProc2.HasExited) { Stop-Process -Id $logcatProc2.Id -Force -ErrorAction SilentlyContinue }

    # -- Step 9: Verify multiplayer state --
    Write-Status ""
    Write-Status "--- Phase 9: Verify multiplayer state ---" "White"

    # Give the game a moment to fully sync -- poll for introspection readiness
    $null = Wait-ForCondition -Description "Introspection ready after game sync" -TimeoutSec 15 -PollMs 1000 -Condition {
        $gi = Get-GameIntrospection -Serial $EMU1
        return ($gi -and $gi.in_game -eq $true)
    }

    $gi1 = Get-GameIntrospection -Serial $EMU1
    $gi2 = Get-GameIntrospection -Serial $EMU2
    if (-not $gi1 -or -not $gi2) { throw "Both live engine snapshots are required for multiplayer qualification" }

    Write-Status "Player 1 game state:"
    Write-Status "  game_mode=$($gi1.game_mode) is_network=$($gi1.is_network) in_game=$($gi1.in_game)"

    $failures = @()

    if (-not $gi1.in_game) { $failures += "Player 1 left the game before verification" }
    if (-not $gi2.in_game) { $failures += "Player 2 left the game before verification" }
    # Verify network mode
    if (-not $gi1.is_network) { $failures += "Player 1 not in network mode" }
    if (-not $gi2.is_network) { $failures += "Player 2 not in network mode" }

    # Verify multiplayer block exists
    if (-not $gi1.multiplayer) {
        $failures += "Player 1 has no multiplayer section"
    } else {
        $mp1state = $gi1.multiplayer
        Write-Status "Player 1 multiplayer state:"
        Write-Status "  num_players=$($mp1state.num_players) gamemode_name=$($mp1state.gamemode_name)"
        Write-Status "  mission=$($mp1state.mission_title) level=$($mp1state.level_num)"

        if ($mp1state.num_players -lt 2) { $failures += "Player 1 sees $($mp1state.num_players) players (expected 2+)" }
        # Engine reports full name (e.g. "cooperative") while lobby uses short name ("coop")
        $modeAliases = @{ "coop" = "cooperative"; "ctf" = "capture_flag" }
        $expectedModes = @($MODE)
        if ($modeAliases.ContainsKey($MODE)) { $expectedModes += $modeAliases[$MODE] }
        if ($mp1state.gamemode_name -notin $expectedModes) { $failures += "Player 1 gamemode: $($mp1state.gamemode_name) (expected $MODE)" }

        # Check callsigns
        $p1Callsigns = ($mp1state.players | ForEach-Object { $_.callsign }) -join ", "
        Write-Status "  Player callsigns: $p1Callsigns"

        $hasHost = $mp1state.players | Where-Object { $_.callsign -eq $CALLSIGN1 }
        $hasJoiner = $mp1state.players | Where-Object { $_.callsign -eq $CALLSIGN2 }
        if (-not $hasHost) { $failures += "Player 1 doesn't see $CALLSIGN1 in player list" }
        if (-not $hasJoiner) { $failures += "Player 1 doesn't see $CALLSIGN2 in player list" }
    }

    if (-not $gi2.multiplayer) {
        $failures += "Player 2 has no multiplayer section"
    } else {
        $mp2state = $gi2.multiplayer
        Write-Status "Player 2 multiplayer state:"
        Write-Status "  num_players=$($mp2state.num_players) gamemode_name=$($mp2state.gamemode_name)"

        if ($mp2state.num_players -lt 2) { $failures += "Player 2 sees $($mp2state.num_players) players (expected 2+)" }

        $p2Callsigns = ($mp2state.players | ForEach-Object { $_.callsign }) -join ", "
        Write-Status "  Player callsigns: $p2Callsigns"

        $hasHost = $mp2state.players | Where-Object { $_.callsign -eq $CALLSIGN1 }
        $hasJoiner = $mp2state.players | Where-Object { $_.callsign -eq $CALLSIGN2 }
        if (-not $hasHost) { $failures += "Player 2 doesn't see $CALLSIGN1 in player list" }
        if (-not $hasJoiner) { $failures += "Player 2 doesn't see $CALLSIGN2 in player list" }
    }



    # -- Phase 10: Optional sustained connectivity soak --
    if ($failures.Count -eq 0 -and $SoakSeconds -gt 0) {
        Write-Status ""
        Write-Status "--- Phase 10: Sustained connectivity check (${SoakSeconds}s) ---" "White"
        $sustainStart = [System.Diagnostics.Stopwatch]::StartNew()
        $sustainFailed = $false
        $prevState = @{}  # track per-player state changes
        while ($sustainStart.Elapsed.TotalSeconds -lt $SoakSeconds -and -not $sustainFailed) {
            Start-Sleep -Seconds 5
            $elapsed = [int]$sustainStart.Elapsed.TotalSeconds
            $sg1 = Get-GameIntrospection -Serial $EMU1
            $sg2 = Get-GameIntrospection -Serial $EMU2
            foreach ($entry in @(@{Tag = "EMU1"; S = $sg1; Num = 1 }, @{Tag = "EMU2"; S = $sg2; Num = 2 })) {
                $tag = $entry.Tag; $sg = $entry.S; $pNum = $entry.Num
                if (-not $sg) {
                    $failures += "Player $pNum has no live engine snapshot at ${elapsed}s"
                    $sustainFailed = $true
                    continue
                }
                try {

                    # Check for disconnect messagebox: game_window_is_front=false
                    # while in_game=true means a modal is on top (e.g. "Host left the game!")
                    $gwFront = $sg.game_window_is_front
                    $menuSub = ""
                    if ($sg.PSObject.Properties['menu'] -and $sg.menu -and
                        $sg.menu.PSObject.Properties['subtitle'] -and $sg.menu.subtitle) {
                        $menuSub = $sg.menu.subtitle
                    }

                    # Check connected status of all players
                    $disconnectedPeers = @()
                    if ($sg.PSObject.Properties['multiplayer'] -and $sg.multiplayer -and
                        $sg.multiplayer.PSObject.Properties['players'] -and $sg.multiplayer.players) {
                        foreach ($p in $sg.multiplayer.players) {
                            if (-not $p.is_me -and $p.connected -eq 0) {
                                $disconnectedPeers += $p.callsign
                            }
                        }
                    }

                    if ($sg.in_game -and -not $gwFront) {
                        # A modal is covering the game -- likely a disconnect notice
                        $failures += "Player $pNum has disconnect modal at ${elapsed}s (menu='$menuSub')"
                        $sustainFailed = $true
                        Write-Status "  [${elapsed}s] ${tag}: DISCONNECT MODAL: '$menuSub'" "Red"
                        if ($sg.console) {
                            Write-Status "  $tag console:" "Gray"
                            foreach ($c in ($sg.console.lines | Select-Object -Last 20)) {
                                Write-Status "    $($c.text)" "Yellow"
                            }
                        }
                        # Dump logcat for both emulators to catch proxy lifecycle events
                        Write-Status "  --- Logcat dump (last 80 lines, proxy/WS tags) ---" "Gray"
                        foreach ($emuEntry in @(@{Tag = "EMU1"; Serial = $EMU1 }, @{Tag = "EMU2"; Serial = $EMU2 })) {
                            $lcOut = & $ADB -s $emuEntry.Serial logcat -d -s "LocalhostProxy:*" "MatchmakingService:*" 2>&1 | Out-String
                            Write-Status "  $($emuEntry.Tag) proxy/WS logcat:" "Gray"
                            foreach ($lcLine in ($lcOut -split "`n" | Select-Object -Last 40)) {
                                if ($lcLine.Trim()) { Write-Status "    $($lcLine.Trim())" "Yellow" }
                            }
                        }
                    } elseif ($disconnectedPeers.Count -gt 0) {
                        $failures += "Player $pNum peer disconnected at ${elapsed}s ($($disconnectedPeers -join ', '))"
                        $sustainFailed = $true
                        Write-Status "  [${elapsed}s] ${tag}: PEER DISCONNECTED: $($disconnectedPeers -join ', ')" "Red"
                    } elseif ($sg.in_game -and $sg.multiplayer) {
                        $np = $sg.multiplayer.num_players
                        $mp = $sg.multiplayer
                        $myNum = $mp.my_player_num
                        # Build per-player state summary using slot index to avoid callsign confusion
                        $pSummary = ""
                        if ($mp.players) {
                            $parts = @()
                            foreach ($p in $mp.players) {
                                $me = ""; if ($p.is_me) { $me = "*" }
                                $parts += "p$($p.slot)${me}=$($p.callsign):s=$([int]$p.shields)/e=$([int]$p.energy)/sc=$($p.score)/c=$($p.connected)"
                            }
                            $pSummary = " [$($parts -join ', ')]"
                        }
                        Write-Status "  [${elapsed}s] ${tag}: gw_front=$gwFront np=$np my=$myNum$pSummary" "Gray"
                        if ($np -lt 2) {
                            $failures += "Player $pNum lost peer at ${elapsed}s (num_players=$np)"
                            $sustainFailed = $true
                        }
                    } elseif (-not $sg.in_game) {
                        $failures += "Player $pNum left game at ${elapsed}s (screen=$($sg.screen_mode))"
                        $sustainFailed = $true
                    }
                } catch {
                    $failures += "Player $pNum introspection failed at ${elapsed}s"
                    $sustainFailed = $true
                    Write-Status "  [${elapsed}s] ${tag}: introspect parse error: $_" "Yellow"
                }
            }
        }
        if (-not $sustainFailed) {
            Write-Status "  Connection sustained for ${SoakSeconds}s" "Green"

            # Final assertion: verify no disconnect dialog appeared in the last moments
            foreach ($entry in @(@{Tag = "EMU1"; Serial = $EMU1; Num = 1 }, @{Tag = "EMU2"; Serial = $EMU2; Num = 2 })) {
                $sg = Get-GameIntrospection -Serial $entry.Serial
                if (-not $sg -or -not $sg.in_game) {
                    $failures += "Player $($entry.Num) has no live game at end of soak"
                } elseif (-not $sg.game_window_is_front) {
                    $menuSub = ""
                    if ($sg.PSObject.Properties['menu'] -and $sg.menu -and
                        $sg.menu.PSObject.Properties['subtitle'] -and $sg.menu.subtitle) {
                        $menuSub = $sg.menu.subtitle
                    }
                    $failures += "Player $($entry.Num) has disconnect modal at end (menu='$menuSub')"
                    Write-Status "  $($entry.Tag): DISCONNECT MODAL at end: '$menuSub'" "Red"
                }
            }
        }
    }

    if ($PublicShareware) {
        foreach ($entry in @(@{ Serial = $EMU1; Local = $CALLSIGN1; Remote = $CALLSIGN2 }, @{ Serial = $EMU2; Local = $CALLSIGN2; Remote = $CALLSIGN1 })) {
            $snapshot = Get-GameIntrospection -Serial $entry.Serial
            if (-not $snapshot -or -not $snapshot.in_game -or -not $snapshot.is_network) {
                throw 'Public online qualification requires live native game snapshots'
            }
            $mp = $snapshot.multiplayer
            $players = @($mp.players)
            if ($mp.num_players -ne 2 -or $mp.level_num -ne 1 -or $mp.mission_title -cne $publicConfig.MissionTitle -or
                @($players | Where-Object { $_.callsign -ieq $entry.Local -and $_.is_me -eq $true -and $_.connected -eq 1 }).Count -ne 1 -or
                @($players | Where-Object { $_.callsign -ieq $entry.Remote -and $_.is_me -eq $false -and $_.connected -eq 1 }).Count -ne 1) {
                throw 'Public online roster, mission or level differs from pinned native fixture'
            }
            $nativeLog = Adb-Dev-Timeout -Serial $entry.Serial -AdbArgs @('logcat', '-d', '-s', 'MainActivity:I', '*:S') -Seconds 10
            $otherGame = if ($Game -eq 'd1') { 'd2' } else { 'd1' }
            if ($nativeLog -notmatch "Loaded native library: dxx-redux-$Game" -or $nativeLog -match "Loaded native library: dxx-redux-$otherGame") {
                throw "Public online fixture did not prove native $Game engine selection"
            }
        }
        Assert-OnlinePeerMatrix -Serials @($EMU1, $EMU2) -Callsigns @($CALLSIGN1, $CALLSIGN2) -Slots @(0, 1) -Phase 'two-player loopback development identity'
        if ($PublicDirect) {
            foreach ($serial in @($EMU1, $EMU2)) {
                $control = Get-MpIntrospection -Serial $serial
                $resources = $control.online_resources
                $routes = @($resources.peers)
                foreach ($route in $routes) {
                    Write-Status "ONLINE_DIRECT_ROUTE slot=$([int]$route.slot) direct=$($route.route_kind -ceq 'direct') epoch=$([long]$route.route_epoch) gameplay=$([long]$route.gameplay_received)" 'Gray'
                }
                if ($routes.Count -ne 1 -or $routes[0].route_kind -cne 'direct' -or $routes[0].route_epoch -le 0 -or
                    $routes[0].route_epoch -ne $resources.runtime_attempt_epoch) { throw 'Direct qualification has no current native direct route' }
            }
            $direct = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2
            if ($direct.relay_sessions -ne 0) { throw 'Requested native direct-path qualification fell back to relay' }
            Write-Status 'ONLINE_EVIDENCE native direct pair advanced with zero relay leases' 'Green'
        }

        if ($TransportMode) {
            $epochs = Assert-OnlineTransportGraph
            if (-not (Invoke-CoopDeathScenario)) { throw 'Shared co-op death/respawn scenario failed' }
            Assert-OnlinePeerMatrix -Serials $allTargets -Callsigns @($CALLSIGN1, $CALLSIGN2) -Slots @(0, 1) -Phase 'shared co-op death/respawn'
            Assert-OnlineTransportGraph -ExpectedEpochs $epochs | Out-Null
            Send-MpCommand -Serial $EMU1 -Command 'disconnect'
            if (-not (Wait-ForCondition -Description 'Online host closure retires joiner routes' -TimeoutSec 20 -PollMs 500 -Condition {
                        $control = Get-MpIntrospection -Serial $EMU2
                        return $control -and -not $control.PSObject.Properties['lobby'] -and $control.online_resources.proxy_workers -eq 0 -and @($control.online_resources.peers).Count -eq 0
                    })) { throw 'Online host closure retained joiner routes' }
            Assert-OnlineClean
            Write-Status "ONLINE_TRANSPORT mode=$TransportMode game=$Game shared_coop_death=True host_closure=True" 'Green'
        } else {
            Join-RunningOnlineGame -Serial $EMU3 -Callsign $CALLSIGN3 -NativeTimeout
            Join-RunningOnlineGame -Serial $EMU3 -Callsign $CALLSIGN3 -CancelAdmission
            Invoke-OnlineReturnToGame
            Join-RunningOnlineGame -Serial $EMU3 -Callsign $CALLSIGN3 -DelayLaunchSeconds 30
            Assert-OnlinePeerMatrix -Serials $allTargets -Callsigns @($CALLSIGN1, $CALLSIGN2, $CALLSIGN3) -Slots @(0, 1, 2) -Phase 'three-player late join'
            $admitted = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2
            if ($admitted.pending_native_admissions -ne 0) { throw 'Native host acceptance did not retire the delayed admission lease' }
            Write-Status 'ONLINE_EVIDENCE delayed assignment, real host consent and native synchronization completed host-owned lease' 'Green'

            Send-MpCommand -Serial $EMU2 -Command 'disconnect'
            Adb-Dev-Timeout -Serial $EMU2 -AdbArgs @('shell', 'am', 'force-stop', $PACKAGE) -Seconds 10 | Out-Null
            # Surviving native gameplay must outlive the departed peer's bind timeout
            Start-Sleep -Seconds 12
            Assert-OnlinePeerMatrix -Serials @($EMU1, $EMU3) -Callsigns @($CALLSIGN1, $CALLSIGN3) -Slots @(0, 2) -Phase 'middle-slot departure preserves survivors'
            Join-RunningOnlineGame -Serial $EMU2 -Callsign $CALLSIGN2 -AllowKnownRejoin
            Assert-OnlinePeerMatrix -Serials $allTargets -Callsigns @($CALLSIGN1, $CALLSIGN2, $CALLSIGN3) -Slots @(0, 1, 2) -Phase 'middle-slot reconnect with fresh routes'
            Send-MpCommand -Serial $EMU1 -Command 'disconnect'
            foreach ($emu in @($EMU2, $EMU3)) {
                if (-not (Wait-ForCondition -Description 'Host closure retires client routes' -TimeoutSec 20 -PollMs 500 -Condition {
                            $control = Get-MpIntrospection -Serial $emu
                            return $control -and -not $control.PSObject.Properties['lobby'] -and $control.online_resources.proxy_workers -eq 0 -and @($control.online_resources.peers).Count -eq 0
                        })) { throw 'Host closure left active client routes' }
            }
            Write-Status 'ONLINE_EVIDENCE host closure retired both surviving clients' 'Green'
            foreach ($emu in @($EMU3, $EMU2, $EMU1)) { Send-MpCommand -Serial $emu -Command 'disconnect' }

            $clean = Wait-ForCondition -Description 'No orphan online rooms, leases or sockets' -TimeoutSec 15 -PollMs 250 -Condition {
                $status = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2
                return $status.rooms -eq 0 -and $status.relay_sessions -eq 0 -and $status.pending_and_authenticated_sockets -eq 0
            }
            if (-not $clean) { throw 'Online cleanup did not return owned server resources to baseline' }
            foreach ($emu in $allTargets) {
                $retired = Wait-ForCondition -Description 'Client jobs and sockets retired' -TimeoutSec 15 -PollMs 500 -Condition {
                    $control = Get-MpIntrospection -Serial $emu
                    if (-not $control) { return $false }
                    $resources = $control.online_resources
                    return $control -and $resources.proxy_workers -eq 0 -and $resources.background_jobs -eq 0 -and -not $resources.candidate_socket -and -not $resources.upnp_mapping -and -not $resources.game_state_updates
                }
                if (-not $retired) { throw 'Client resources did not return to baseline' }
            }
            Write-Status 'ONLINE_EVIDENCE zero rooms, relay leases, control sockets and client network workers' 'Green'
            if ($PublicDirect) {
                Start-OnlineRecoveryGame
                Invoke-OnlineBackgroundResume
                $direct = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2
                if ($direct.relay_sessions -ne 0) { throw 'Direct recovery unexpectedly used relay' }
                Set-OnlineFault -Ws 'drop'
                foreach ($serial in @($EMU1, $EMU2)) {
                    if (-not (Wait-ForCondition -Description 'Direct control loss retires routes' -TimeoutSec 20 -PollMs 500 -Condition {
                                $control = Get-MpIntrospection -Serial $serial
                                return $control -and -not $control.PSObject.Properties['lobby'] -and $control.online_resources.proxy_workers -eq 0
                            })) { throw 'Direct control loss retained native routes' }
                }
                Assert-OnlineClean
                Set-OnlineFault
                $serverProcess.Kill()
                if (-not $serverProcess.WaitForExit(5000)) { throw 'Direct fixture server did not stop' }
                $env:FORCE_RELAY = 'true'
                $PublicDirect = $false
                $serverProcess = Start-Process -FilePath $serverBin -WorkingDirectory $serverDir -NoNewWindow -PassThru -RedirectStandardError (Join-Path $REPO_ROOT 'temp/server-relay-error.log') -RedirectStandardOutput (Join-Path $REPO_ROOT 'temp/server-relay.log')
                if (-not (Wait-ForCondition -Description 'Relay fixture readiness' -TimeoutSec 15 -PollMs 250 -Condition {
                            if ($serverProcess.HasExited) { throw 'Relay fixture exited' }
                            try { return (Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 1).ready } catch { return $false }
                        })) { throw 'Relay fixture did not become ready' }
                Write-Status 'ONLINE_EVIDENCE direct background and control-loss recovery completed before separate relay fault qualification' 'Green'
            }
            Start-OnlineRecoveryGame
            Invoke-OnlineBackgroundResume
            Set-OnlineFault -Udp 'impair'
            Assert-OnlinePeerMatrix -Serials @($EMU1, $EMU2) -Callsigns @($CALLSIGN1, $CALLSIGN2) -Slots @(0, 1) -Phase 'UDP delay loss duplication reordering'
            Set-OnlineFault -Udp 'readdress'
            Assert-OnlinePeerMatrix -Serials @($EMU1, $EMU2) -Callsigns @($CALLSIGN1, $CALLSIGN2) -Slots @(0, 1) -Phase 'owned NAT address and port change'
            Set-OnlineFault -Udp 'drop'
            Start-Sleep -Seconds 2
            Set-OnlineFault
            Assert-OnlinePeerMatrix -Serials @($EMU1, $EMU2) -Callsigns @($CALLSIGN1, $CALLSIGN2) -Slots @(0, 1) -Phase 'short UDP-only loss recovery'
            Assert-OnlineUdpOutage
            Set-OnlineFault
            Write-Status 'ONLINE_EVIDENCE long UDP-only outage bounded visible joiner cleanup and terminal or verified live host-only state' 'Green'
            Start-OnlineRecoveryGame
            Set-OnlineFault -Ws 'drop'
            foreach ($serial in @($EMU1, $EMU2)) {
                if (-not (Wait-ForCondition -Description 'Control outage retires game routes' -TimeoutSec 20 -PollMs 500 -Condition {
                            $control = Get-MpIntrospection -Serial $serial
                            return $control -and -not $control.PSObject.Properties['lobby'] -and $control.online_resources.proxy_workers -eq 0
                        })) { throw 'Control loss left active game routes' }
            }
            Assert-OnlineClean
            Set-OnlineFault
            Start-Sleep -Seconds 5
            Assert-OnlineClean -NoDisconnect
            Write-Status 'ONLINE_EVIDENCE WS-only outage and cancellation stopped retries and retired resources' 'Green'
            Start-OnlineRecoveryGame
            $serverProcess.Kill()
            if (-not $serverProcess.WaitForExit(5000)) { throw 'Owned server did not stop' }
            $serverProcess = Start-Process -FilePath $serverBin -WorkingDirectory $serverDir -NoNewWindow -PassThru -RedirectStandardError (Join-Path $REPO_ROOT 'temp/server-restart-error.log') -RedirectStandardOutput (Join-Path $REPO_ROOT 'temp/server-restart.log')
            if (-not (Wait-ForCondition -Description 'Restarted server readiness' -TimeoutSec 15 -PollMs 250 -Condition {
                        if ($serverProcess.HasExited) { throw 'Restarted server exited' }
                        try { return (Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 1).ready } catch { return $false }
                    })) { throw 'Restarted server unavailable' }
            foreach ($serial in @($EMU1, $EMU2)) {
                if (-not (Wait-ForCondition -Description 'Restarted control session without stale room' -TimeoutSec 30 -PollMs 500 -Condition {
                            $control = Get-MpIntrospection -Serial $serial
                            return $control -and $control.status -eq 'CONNECTED' -and -not $control.PSObject.Properties['lobby'] -and $control.online_resources.proxy_workers -eq 0
                        })) { throw 'Server restart retained stale room or routes' }
            }
            Start-OnlineRecoveryGame
            Assert-OnlineClean
            Write-Status 'ONLINE_EVIDENCE server restart retired old game and explicit new game carried native traffic' 'Green'
        }

    }

    # -- Results --
    Write-Status ""
    Write-Status "=== Test Results ===" "White"

    if ($failures.Count -eq 0) {
        $testPassed = $true
        Write-Status "ALL CHECKS PASSED" "Green"
        Write-Status "  - Both players connected to matchmaking server"
        Write-Status "  - Lobby created and joined successfully"
        Write-Status "  - Chat messages exchanged both ways"
        Write-Status "  - Both players readied up"
        Write-Status "  - Game started and launched on both emulators"
        Write-Status "  - Both players in network game with correct player count and callsigns"
    } else {
        Write-Status "FAILURES ($($failures.Count)):" "Red"
        foreach ($f in $failures) {
            Write-Status "  - $f" "Red"
        }
    }

} catch {
    Write-Status "EXCEPTION: $($_.Exception.Message)" "Red"
    $where = $_.InvocationInfo
    if ($where) {
        Write-Status "  at $($where.ScriptName):$($where.ScriptLineNumber)" "Red"
        if ($where.Line) {
            Write-Status "  line: $($where.Line.Trim())" "Red"
        }
    }
    if ($_.ScriptStackTrace) {
        Write-Status "  stack: $($_.ScriptStackTrace)" "Gray"
    }
} finally {
    Cleanup
    if ($testPassed) {
        exit 0
    } else {
        exit 1
    }
}
