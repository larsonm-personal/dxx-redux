param(
    [string]$Mission = 'descent',
    [switch]$PublicShareware,
    [switch]$ThreePlayerStartup,
    [switch]$RecoveryChecks = ($env:DXX_RECOVERY_CHECKS -eq 'true'),
    [switch]$MeasureTransitions = ($env:DXX_MEASURE_TRANSITIONS -eq 'true'),
    [string]$D1DataDirectory,
    [string]$D2DataDirectory
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$hostSerial = $script:PRIMARY_EMULATOR_SERIAL
$clientSerial = $script:SECONDARY_EMULATOR_SERIAL

function Read-LaunchLog([string]$Serial) {
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @('logcat', '-d', '-s', 'DXX-Launcher:I', 'DXX-MP:I', 'LobbyService:I') -Seconds 5
}

function Read-LanPreparationLobby([string]$Serial) {
    $cleared = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sh', '-c',
        "'rm -f files/mp_introspect.json && echo DXX_LAN_STATE_CLEARED'") -Seconds 5
    if ($cleared -cne 'DXX_LAN_STATE_CLEARED') { throw 'Could not clear old LAN lobby evidence' }
    Send-MpCommand -Serial $Serial -Command 'introspect'
    $raw = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/mp_introspect.json') -Seconds 5
    try { return ($raw | ConvertFrom-Json) } catch { return $null }
}

function Test-LanPreparationLobbyPeers {
    param($State, [string]$HostCallsign, [string]$ClientCallsign, [switch]$RequireReady)

    if (-not $State -or -not $State.PSObject.Properties['callsign'] -or -not $State.PSObject.Properties['lan']) { return $false }
    $lan = $State.lan
    if (-not $lan -or -not $lan.PSObject.Properties['hosting'] -or -not $lan.PSObject.Properties['players']) { return $false }
    if ($State.callsign -cne $HostCallsign -or $lan.hosting -ne $true -or $HostCallsign -ceq $ClientCallsign) { return $false }
    $players = @($lan.players)
    if ($players.Count -ne 2) { return $false }
    foreach ($player in $players) {
        if (-not $player -or -not $player.PSObject.Properties['callsign'] -or
            -not $player.PSObject.Properties['connected'] -or -not $player.PSObject.Properties['ready']) { return $false }
        if ($player.connected -ne $true -or ($RequireReady -and $player.ready -ne $true)) { return $false }
    }
    return @($players | Where-Object { $_.callsign -ceq $HostCallsign }).Count -eq 1 -and
    @($players | Where-Object { $_.callsign -ceq $ClientCallsign }).Count -eq 1
}

function Assert-LanPreparationTargets {
    param([string]$HostSerial, [string]$ClientSerial)

    if ($HostSerial -notmatch '^emulator-\d+$' -or $ClientSerial -notmatch '^emulator-\d+$' -or $HostSerial -eq $ClientSerial) {
        throw 'LAN launch preparation requires two distinct disposable emulators; physical targets are not allowed'
    }
    foreach ($serial in @($HostSerial, $ClientSerial)) {
        $online = Adb-Dev-Timeout -Serial $serial -AdbArgs @('get-state') -Seconds 5
        if (([string]$online).Trim() -cne 'device') { throw "$serial is offline" }
        $emulated = Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'getprop', 'ro.kernel.qemu') -Seconds 5
        if (([string]$emulated).Trim() -cne '1') { throw "Refusing to reset unverified emulator $serial" }
    }
}

function Test-PublicLanEngineLog {
    param([string]$Log, [ValidateSet('d1', 'd2')][string]$Engine)

    # Native D1 has no asset_trace; use the same fresh library-load evidence as public first-play
    $otherEngine = if ($Engine -eq 'd1') { 'd2' } else { 'd1' }
    return $Log -match "Loaded native library: dxx-redux-$Engine\b" -and
    $Log -notmatch "Loaded native library: dxx-redux-$otherEngine\b"
}

function Test-PublicLanPeerState {
    param(
        $State, [string]$LocalCallsign, [string]$RemoteCallsign, [bool]$IsHost, [long]$PigSize,
        [ValidateSet('d1', 'd2')][string]$Engine = 'd2',
        [ValidateSet('d1', 'd2')][string]$Content = 'd1',
        [string]$NativeLog
    )

    $missionName = if ($Engine -eq 'd1') { '' } elseif ($Content -eq 'd1') { 'descent' } else { 'd2demo' }
    $missionTitle = if ($Content -eq 'd1') { 'Descent Demo' } else { 'Descent 2 Demo' }
    if (-not $State -or $State.in_game -ne $true -or $State.is_network -ne $true -or
        $State.screen_mode -cne 'game' -or $State.current_level_num -ne 1 -or
        $State.multiplayer.mission_name -cne $missionName -or $State.multiplayer.mission_title -cne $missionTitle -or
        $State.multiplayer.level_num -ne 1 -or $State.multiplayer.gamemode_name -cne 'cooperative' -or
        $State.multiplayer.num_connected -ne 2 -or $State.multiplayer.num_players -ne 2 -or
        $State.multiplayer.i_am_master -ne $IsHost -or -not (Test-PublicLanEngineLog -Log $NativeLog -Engine $Engine)) { return $false }
    if ($Engine -eq 'd1') {
        # Native D1 omits this property; inspect it without a StrictMode missing-property read
        $assetTrace = $null
        if ($State -is [Collections.IDictionary]) {
            $assetTrace = $State['asset_trace']
        } elseif ($State.PSObject.Properties['asset_trace']) {
            $assetTrace = $State.PSObject.Properties['asset_trace'].Value
        }
        if ($Content -cne 'd1' -or $null -ne $assetTrace) { return $false }
    } else {
        $mode = if ($Content -eq 'd1') { 'd1-in-d2' } else { 'd2' }
        if ($State.asset_trace.runtime_engine -cne 'd2' -or $State.asset_trace.selected_base_game -cne $Content -or
            $State.asset_trace.mode -cne $mode) { return $false }
        if ($Content -eq 'd1' -and ($State.asset_trace.d1_compat.native_assets -ne $true -or
                $State.asset_trace.d1_compat.sound_pig_size -ne $PigSize -or $State.asset_trace.d1_compat.sound_files -ne 70)) { return $false }
    }
    $players = @($State.multiplayer.players)
    $local = @($players | Where-Object { $_.is_me -eq $true -and $_.callsign -ieq $LocalCallsign -and $_.connected -eq 1 })
    $remote = @($players | Where-Object { $_.is_me -eq $false -and $_.callsign -ieq $RemoteCallsign -and $_.connected -eq 1 })
    return $players.Count -eq 2 -and $local.Count -eq 1 -and $remote.Count -eq 1
}

function Assert-PublicLanDeviceAssets {
    param([string]$Serial, [object[]]$Files = $publicFiles)

    foreach ($file in $Files) {
        $devicePath = "$($script:DEFAULT_SET_DIR)/$($file.File)"
        $actual = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'sha256sum', $devicePath) -Seconds 30
        if ($actual -cne "$($file.Sha256)  $devicePath") { throw "Pinned public asset differs on ${Serial}: $($file.File)" }
    }
}

function Stage-PublicLanAssets {
    param([string]$Serial, [object[]]$Files = $publicFiles)

    # The caller verifies BOTH disposable targets before any reset; no retail or external-set fallback survives
    $cleared = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'pm', 'clear', $script:PACKAGE) -Seconds 30
    if ($cleared -ne 'Success') { throw "Could not clear disposable emulator app state on $Serial" }
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'mkdir', '-p', $script:DEFAULT_SET_DIR) -Seconds 5 | Out-Null
    foreach ($file in $Files) {
        $staged = "/data/local/tmp/lan-public-$($file.File)"
        try {
            Adb-Dev-Timeout -Serial $Serial -AdbArgs @('push', $file.Source, $staged) -Seconds 30 | Out-Null
            Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cp', $staged, "$($script:DEFAULT_SET_DIR)/$($file.File)") -Seconds 10 | Out-Null
        } finally {
            Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'rm', '-f', $staged) -Seconds 5 | Out-Null
        }
    }
    Assert-PublicLanDeviceAssets -Serial $Serial -Files $Files
}

function Invoke-D1OnlyLanDiagnostic {
    # Record current admission separately; a rejection is evidence, never a playable-start pass
    $previousSerial = $env:ANDROID_SERIAL
    try {
        $d1Files = @($publicFiles | Where-Object Game -CEQ 'd1')
        foreach ($serial in @($hostSerial, $clientSerial)) {
            $files = if ($serial -eq $hostSerial) { $publicFiles } else { $d1Files }
            Stage-PublicLanAssets -Serial $serial -Files $files
            Reset-DeviceGameState -Serial $serial
            if (-not (Start-SetupActivity -Serial $serial)) { throw "D1-only launcher did not start on $serial" }
            $env:ANDROID_SERIAL = $serial
            $ready = Wait-ForCondition -Description "D1-only assets ready on $serial" -TimeoutSec 60 -Condition {
                $setup = Get-SetupIntrospection
                $setup | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "d1-only-$serial-setup.json") -Encoding utf8
                $setup.d1.ready -eq $true -and $setup.d1_in_d2.ready -eq $true -and
                $setup.d2.ready -eq ($serial -eq $hostSerial)
            }
            if (-not $ready) { throw "D1-only fixture readiness was not verified on $serial" }
        }
        $hostIp = Get-DeviceWlanIp -Serial $hostSerial
        if (-not $hostIp) { throw 'D1-only diagnostic requires the host shared-Wi-Fi address' }
        Send-MpCommand -Serial $hostSerial -Command 'lan_host_lobby' -Extras @('--es', 'callsign', 'D1ProbeH', '--es', 'game', 'd2', '--es', 'mission', 'descent')
        $hosting = Wait-ForCondition -Description 'D1 probe host lobby is active' -TimeoutSec 30 -Condition {
            $state = Read-LanPreparationLobby -Serial $hostSerial
            $state | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory 'd1-only-host-lobby.json') -Encoding utf8
            $state -and $state.callsign -ceq 'D1ProbeH' -and $state.lan.hosting -eq $true
        }
        if (-not $hosting) { throw 'D1-only diagnostic host did not create a lobby' }
        Send-MpCommand -Serial $clientSerial -Command 'lan_discover' -Extras @('--es', 'callsign', 'D1ProbeJ')
        $observed = Wait-ForCondition -Description 'D1-only LAN admission or explicit rejection' -TimeoutSec 30 -PollMs 1000 -Condition {
            Send-MpCommand -Serial $clientSerial -Command 'lan_join_first_lobby' -Extras @('--es', 'host_addr', $hostIp)
            $state = Read-LanPreparationLobby -Serial $clientSerial
            if (-not $state) { return $false }
            $state | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory 'd1-only-client-lobby.json') -Encoding utf8
            if ($state.lan.joined_host -eq $hostIp -and $state.lan.joined_lobby_id) {
                $result = @{ outcome = 'admitted_to_lobby'; gameplay_tested = $false; engine = 'd2'; mission = 'descent' }
            } elseif (@($state.lan.discovered | Where-Object { $_.host_address -eq $hostIp -and $_.game -ceq 'd2' }).Count -eq 1 -and
                $state.lan.diagnostics -like 'Cannot join: Descent 2 game data is not ready.*') {
                $result = @{ outcome = 'blocked_by_engine_readiness'; gameplay_tested = $false; engine = 'd2'; mission = 'descent'; message = $state.lan.diagnostics }
            } else { return $false }
            $result | ConvertTo-Json | Set-Content (Join-Path $outputDirectory 'd1-only-admission.json') -Encoding utf8
            Write-Host "DIAGNOSTIC: D1-only LAN $($result.outcome); native D1 and full public baseline sessions follow"
            return $true
        }
        if (-not $observed) { throw 'D1-only LAN candidate produced neither admission nor an explicit engine-readiness rejection' }
    } finally {
        foreach ($serial in @($hostSerial, $clientSerial)) {
            Read-LaunchLog $serial | Out-File (Join-Path $outputDirectory "d1-only-$serial.log") -Encoding utf8
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
        }
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}

function Invoke-LanPreparationSession {
    param(
        [string]$HostSerial, [string]$ClientSerial, [int]$Session = 1,
        [ValidateSet('d1', 'd2')][string]$Engine = 'd2',
        [ValidateSet('d1', 'd2')][string]$Content = 'd1',
        [AllowEmptyString()][string]$SessionMission = $Mission,
        [object[]]$AssetFiles = $publicFiles
    )

    $hostCallsign = if ($PublicShareware) { "PrepH$Session" } else { 'PrepHost' }
    $clientCallsign = if ($PublicShareware) { "PrepJ$Session" } else { 'PrepJoin' }
    $startupTimeout = if ($PublicShareware) { Get-AndroidInfrastructureTimeout -DefaultSeconds 120 -Kind GameStartup -Serial $HostSerial } else { 30 }
    try {
        foreach ($serial in @($HostSerial, $ClientSerial)) {
            Reset-DeviceGameState -Serial $serial
            if (-not (Start-SetupActivity -Serial $serial)) { throw "Launcher did not start on $serial" }
            if (Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5) {
                throw "Previous game process survived the session reset on $serial"
            }
            $cleared = Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'sh', '-c',
                "'logcat -c && echo DXX_LAN_LOG_CLEARED'") -Seconds 5
            if ($cleared -cne 'DXX_LAN_LOG_CLEARED') { throw "Could not clear old LAN evidence on $serial" }
        }
        # Reuse the QR owner's shell quoting for native D1's empty built-in mission key
        $missionArgument = if ($SessionMission) { $SessionMission } else { "''" }
        $hostExtras = @('--es', 'callsign', $hostCallsign, '--es', 'game', $Engine, '--es', 'mission', $missionArgument)
        # Native auto-host starts when the configured player capacity is reached
        if ($PublicShareware) { $hostExtras += @('--ei', 'max_players', '2') }
        Send-MpCommand -Serial $HostSerial -Command 'lan_host_lobby' -Extras $hostExtras
        Send-MpCommand -Serial $ClientSerial -Command 'lan_discover' -Extras @('--es', 'callsign', $clientCallsign)
        $joined = Wait-ForCondition -Description 'client joined launcher lobby' -TimeoutSec 25 -PollMs 500 -Condition {
            Send-MpCommand -Serial $ClientSerial -Command 'lan_join_first_lobby'
            $state = Read-LanPreparationLobby -Serial $HostSerial
            $state | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "session-$Session-host-joined-lobby.json") -Encoding utf8
            Test-LanPreparationLobbyPeers -State $state -HostCallsign $hostCallsign -ClientCallsign $clientCallsign
        }
        if (-not $joined) { throw 'Client did not join launcher lobby' }
        $ready = Wait-ForCondition -Description 'both peers ready' -TimeoutSec 20 -PollMs 500 -Condition {
            Send-MpCommand -Serial $ClientSerial -Command 'lan_set_ready' -Extras @('--ez', 'ready', 'true')
            $state = Read-LanPreparationLobby -Serial $HostSerial
            $state | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "session-$Session-host-ready-lobby.json") -Encoding utf8
            Test-LanPreparationLobbyPeers -State $state -HostCallsign $hostCallsign -ClientCallsign $clientCallsign -RequireReady
        }
        if (-not $ready) { throw 'Lobby did not become ready' }
        # Do not consume the host launch event yet: this models slow host preparation
        Send-MpCommand -Serial $HostSerial -Command 'lan_start_game' -Extras @('--ei', 'level_num', '1')
        $prepared = Wait-ForCondition -Description 'client prepares before host Activity start' -TimeoutSec $startupTimeout -PollMs 500 -Condition {
            Send-MpCommand -Serial $ClientSerial -Command 'launch_game'
            (Read-LaunchLog $ClientSerial) -match 'mp preparation role=client .*step=mission_ready'
        }
        if (-not $prepared) { throw 'Client did not prepare while host launch was pending' }
        if (Adb-Dev-Timeout -Serial $ClientSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5) {
            throw 'Client entered the engine before host confirmation'
        }
        Send-MpCommand -Serial $HostSerial -Command 'launch_game'
        $launched = Wait-ForCondition -Description 'both prepared peers enter engine' -TimeoutSec $startupTimeout -PollMs 500 -Condition {
            $clientLog = Read-LaunchLog $ClientSerial
            $hostPid = Adb-Dev-Timeout -Serial $HostSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5
            $clientPid = Adb-Dev-Timeout -Serial $ClientSerial -AdbArgs @('shell', 'pidof', "$($script:PACKAGE):game") -Seconds 5
            $hostPid -and $clientPid -and $clientLog -match 'mp preparation role=client .*step=launch_released'
        }
        if (-not $launched) { throw 'Host confirmation did not release prepared client' }
        $log = Read-LaunchLog $ClientSerial
        if (([regex]::Matches($log, 'mp preparation role=client .*step=begin')).Count -ne 1) { throw 'Client prepared more than once' }
        if ($PublicShareware) {
            $lastPeerEvidence = @{}
            $playing = Wait-ForCondition -Description "public LAN session $Session ($Engine/$Content) has two live level-one peers" -TimeoutSec $startupTimeout -PollMs 1000 -Condition {
                $hostState = Get-GameIntrospection -Serial $HostSerial -Fresh
                $clientState = Get-GameIntrospection -Serial $ClientSerial -Fresh
                $hostState | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "session-$Session-host.json") -Encoding utf8
                $clientState | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "session-$Session-client.json") -Encoding utf8
                $hostNativeLog = Adb-Dev-Timeout -Serial $HostSerial -AdbArgs @('logcat', '-d', '-s', 'MainActivity:I', '*:S') -Seconds 5
                $clientNativeLog = Adb-Dev-Timeout -Serial $ClientSerial -AdbArgs @('logcat', '-d', '-s', 'MainActivity:I', '*:S') -Seconds 5
                $hostNativeLog | Set-Content (Join-Path $outputDirectory "session-$Session-host-native-library.log") -Encoding utf8
                $clientNativeLog | Set-Content (Join-Path $outputDirectory "session-$Session-client-native-library.log") -Encoding utf8
                $lastPeerEvidence.host = @{ State = $hostState; NativeLog = $hostNativeLog }
                $lastPeerEvidence.client = @{ State = $clientState; NativeLog = $clientNativeLog }
                (Test-PublicLanPeerState -State $hostState -LocalCallsign $hostCallsign -RemoteCallsign $clientCallsign -IsHost $true -PigSize $pigSize -Engine $Engine -Content $Content -NativeLog $hostNativeLog) -and
                (Test-PublicLanPeerState -State $clientState -LocalCallsign $clientCallsign -RemoteCallsign $hostCallsign -IsHost $false -PigSize $pigSize -Engine $Engine -Content $Content -NativeLog $clientNativeLog)
            }
            if (-not $playing) {
                foreach ($peer in @('host', 'client')) {
                    $state = $lastPeerEvidence[$peer].State
                    $diagnostic = [ordered]@{
                        session = $Session; peer = $peer; engine = $Engine; content = $Content; has_state = $null -ne $state
                        native_library_match = Test-PublicLanEngineLog -Log $lastPeerEvidence[$peer].NativeLog -Engine $Engine
                        in_game = $state.in_game; is_network = $state.is_network; screen_mode = $state.screen_mode
                        level = $state.current_level_num; mission = $state.multiplayer.mission_name; title = $state.multiplayer.mission_title
                        mode = $state.multiplayer.gamemode_name; network_level = $state.multiplayer.level_num
                        players = $state.multiplayer.num_players; connected = $state.multiplayer.num_connected
                        capacity = $state.multiplayer.max_players; is_host = $state.multiplayer.i_am_master
                        roster = @($state.multiplayer.players | Select-Object -First 4 -Property callsign, is_me, connected)
                    }
                    Write-Host "LAN final peer evidence: $($diagnostic | ConvertTo-Json -Depth 5 -Compress)"
                }
                throw "Public LAN session $Session never reached matching two-player $Engine/$Content gameplay"
            }
            foreach ($serial in @($HostSerial, $ClientSerial)) { Assert-PublicLanDeviceAssets -Serial $serial -Files $AssetFiles }
            [ordered]@{
                result = 'PASS'; session = $Session; engine = $Engine; content = $Content; mission = $SessionMission; level = 1
                host = $HostSerial; client = $ClientSerial; host_callsign = $hostCallsign; client_callsign = $clientCallsign
                connected_players_per_peer = 2; engine_identity = 'fresh native-library log and fresh gameplay introspection'
            } | ConvertTo-Json | Set-Content (Join-Path $outputDirectory "session-$Session-summary.json") -Encoding utf8
            Write-Output "PASS: public LAN session $Session ($Engine/$Content) reached level 1 with two connected players on both peers"
        } else {
            Write-Output 'PASS: D1-in-D2 client prepared before host launch, waited for confirmation, then both entered the engine'
        }
    } finally {
        foreach ($serial in @($HostSerial, $ClientSerial)) {
            Read-LaunchLog $serial | Out-File (Join-Path $outputDirectory "session-$Session-$serial.log") -Encoding utf8
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('logcat', '-d') -Seconds 10 |
                Out-File (Join-Path $outputDirectory "session-$Session-$serial-logcat.txt") -Encoding utf8
            Adb-Dev-Timeout -Serial $serial -AdbArgs @('shell', 'am', 'force-stop', $script:PACKAGE) -Seconds 5 | Out-Null
        }
    }
}

if (($D1DataDirectory -or $D2DataDirectory) -and -not $PublicShareware) { throw 'Data directory overrides require -PublicShareware' }
if ($ThreePlayerStartup -and (-not $PublicShareware -or $RecoveryChecks -or $MeasureTransitions)) { throw 'Three-player startup is a separate public suite, not a timing or recovery matrix' }
if ($PublicShareware -and $Mission -cne 'descent') { throw 'The public LAN variant requires mission descent' }
# Reject phone overrides before the first reset, including when either target is invalid
Assert-LanPreparationTargets -HostSerial $hostSerial -ClientSerial $clientSerial
if ($PublicShareware) {
    . "$PSScriptRoot/../helpers/shareware_test_data.ps1"
    $publicConfig = Get-SharewareTestGameConfig -RepoRoot $script:REPO_ROOT -Game d1
    $D1DataDirectory = Resolve-SharewareTestData -RepoRoot $script:REPO_ROOT -Game d1 -DataDir $D1DataDirectory
    $pigSize = @($publicConfig.RequiredHashes | Where-Object File -CEQ 'descent.pig')[0].Size
    # LAN joins require the host engine's base files, even when its mission uses D1 content
    $publicD2Config = Get-SharewareTestGameConfig -RepoRoot $script:REPO_ROOT -Game d2
    $D2DataDirectory = Resolve-SharewareTestData -RepoRoot $script:REPO_ROOT -Game d2 -DataDir $D2DataDirectory
    $publicFiles = @(foreach ($profile in @(
                @{ Config = $publicConfig; Directory = $D1DataDirectory },
                @{ Config = $publicD2Config; Directory = $D2DataDirectory }
            )) {
            foreach ($file in $profile.Config.RequiredHashes) {
                @{ Game = $profile.Config.Game; File = $file.File; Sha256 = $file.Sha256; Source = (Join-Path $profile.Directory $file.File) }
            }
        })
}
$evidenceName = if ($ThreePlayerStartup) { 'lan-three-player-startup' } else { "lan-launch-preparation-$(if ($PublicShareware) { 'public-shareware' } else { 'retail' })" }
$outputDirectory = Join-Path $PSScriptRoot "../temp/$evidenceName"
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $outputDirectory
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
Write-Output "LAN launch evidence: $outputDirectory"
if ($ThreePlayerStartup -or $MeasureTransitions -or $RecoveryChecks) {
    if (-not $PublicShareware) { throw 'Transition measurements require public disposable fixtures' }
    . "$PSScriptRoot/../helpers/measure_lan_transitions.ps1"
    $latencyScripts = @{
        skip = 'test_lan_latency_skip.jsonc'
        exit = 'test_lan_latency_exit.jsonc'
        next = 'test_lan_latency_next.jsonc'
    }
    if ($ThreePlayerStartup) {
        $script:latencyClock = [Diagnostics.Stopwatch]::StartNew()
        . "$PSScriptRoot/../helpers/check_lan_parallel_startup.ps1"
        . "$PSScriptRoot/../helpers/check_lan_three_player_startup.ps1"
        Invoke-LanThreePlayerStartupChecks
        return
    }
    if ($RecoveryChecks) {
        $script:latencyClock = [Diagnostics.Stopwatch]::StartNew()
        . "$PSScriptRoot/../helpers/check_lan_startup_recovery.ps1"
        Invoke-LanStartupRecoveryChecks
        return
    }
    Invoke-LanTransitionMeasurements
    . "$PSScriptRoot/../helpers/check_lan_parallel_startup.ps1"
    Invoke-LanParallelStartupChecks
    return
}
if ($PublicShareware) {
    Invoke-D1OnlyLanDiagnostic
    # Restore D1-only host assets before the native D1 session
    Stage-PublicLanAssets -Serial $hostSerial -Files @($publicFiles | Where-Object Game -CEQ 'd1')
    Invoke-LanPreparationSession -HostSerial $hostSerial -ClientSerial $clientSerial -Session 1 -Engine d1 -Content d1 -SessionMission '' -AssetFiles @($publicFiles | Where-Object Game -CEQ 'd1')
    Write-Output 'Staging both pinned public base asset sets for native D2 and imported-D1 LAN starts'
    foreach ($serial in @($hostSerial, $clientSerial)) { Stage-PublicLanAssets -Serial $serial }
    # Cold rehost on the other emulator exercises both roles without a redundant fourth session
    Invoke-LanPreparationSession -HostSerial $clientSerial -ClientSerial $hostSerial -Session 2 -Engine d2 -Content d2 -SessionMission 'd2demo'
    Invoke-LanPreparationSession -HostSerial $hostSerial -ClientSerial $clientSerial -Session 3 -Engine d2 -Content d1 -SessionMission 'descent'
} else {
    # Earlier public cases deliberately cleared retail assets on these disposable peers
    foreach ($serial in @($hostSerial, $clientSerial)) {
        if (-not (Ensure-StandardGameDataOnDevice -Serial $serial)) {
            throw "Could not provision retail LAN fixture on $serial"
        }
    }
    Invoke-LanPreparationSession -HostSerial $hostSerial -ClientSerial $clientSerial
}
