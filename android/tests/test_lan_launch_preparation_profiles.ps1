#!/usr/bin/env pwsh
# Host-only negative controls for public LAN evidence and disposable-target admission
$ErrorActionPreference = 'Stop'
$runnerPath = Join-Path $PSScriptRoot 'test_lan_launch_preparation.ps1'
$parseErrors = $null
$runner = [Management.Automation.Language.Parser]::ParseFile($runnerPath, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "LAN owner has parser errors: $parseErrors" }
$mission = $runner.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -eq 'Mission' }
$public = $runner.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -eq 'PublicShareware' }
if ($mission.DefaultValue.SafeGetValue() -cne 'descent' -or -not $public -or $public.DefaultValue) {
    throw 'Retail launch preparation must remain the default; public staging requires an explicit switch'
}
# Execute only the owner's host-argument construction, without any device commands
$hostArguments = $runner.Find({ param($node)
        $node -is [Management.Automation.Language.AssignmentStatementAst] -and
        $node.Left.Extent.Text -ceq '$hostExtras' -and $node.Operator -eq 'Equals'
    }, $true)
$publicCapacity = $runner.Find({ param($node)
        $node -is [Management.Automation.Language.IfStatementAst] -and
        $node.Extent.Text -ceq 'if ($PublicShareware) { $hostExtras += @(''--ei'', ''max_players'', ''2'') }'
    }, $true)
if (-not $hostArguments -or -not $publicCapacity) { throw 'Public two-player capacity must be explicitly scoped to the public variant' }
$buildHostArguments = [scriptblock]::Create($hostArguments.Extent.Text + "`n" + $publicCapacity.Extent.Text + "`nreturn ,`$hostExtras")
foreach ($PublicShareware in @($false, $true)) {
    foreach ($Engine in @('d1', 'd2')) {
        $hostCallsign = 'PrepH1'
        $missionArgument = if ($Engine -eq 'd1') { "''" } else { 'descent' }
        $actual = & $buildHostArguments
        $expected = @('--es', 'callsign', $hostCallsign, '--es', 'game', $Engine, '--es', 'mission', $missionArgument)
        if ($PublicShareware) { $expected += @('--ei', 'max_players', '2') }
        if (($actual | ConvertTo-Json -Compress) -cne ($expected | ConvertTo-Json -Compress)) {
            throw "Host arguments changed unexpectedly: public=$PublicShareware engine=$Engine"
        }
    }
}
foreach ($name in @('Read-LanPreparationLobby', 'Test-LanPreparationLobbyPeers', 'Test-PublicLanEngineLog', 'Test-PublicLanPeerState', 'Assert-LanPreparationTargets', 'Assert-PublicLanDeviceAssets', 'Stage-PublicLanAssets')) {
    $definition = $runner.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $true)
    if (-not $definition) { throw "LAN owner function missing: $name" }
    . ([scriptblock]::Create($definition.Extent.Text))
}

function New-PeerState {
    param([string]$Engine = 'd2', [string]$Content = 'd1')
    $state = @{
        in_game = $true; is_network = $true; screen_mode = 'game'; current_level_num = 1
        asset_trace = @{
            runtime_engine = 'd2'; selected_base_game = 'd1'; mode = 'd1-in-d2'
            d1_compat = @{ native_assets = $true; sound_pig_size = 5092871; sound_files = 70 }
        }
        multiplayer = @{
            mission_name = 'descent'; mission_title = 'Descent Demo'; level_num = 1; gamemode_name = 'cooperative'
            num_players = 2; num_connected = 2; i_am_master = $true
            players = @(
                @{ callsign = 'preph1'; is_me = $true; connected = 1 },
                @{ callsign = 'prepj1'; is_me = $false; connected = 1 }
            )
        }
    }
    if ($Engine -eq 'd1') {
        $state.Remove('asset_trace')
        $state.multiplayer.mission_name = ''
    } elseif ($Content -eq 'd2') {
        $state.asset_trace.selected_base_game = 'd2'
        $state.asset_trace.mode = 'd2'
        $state.asset_trace.d1_compat.native_assets = $false
        $state.multiplayer.mission_name = 'd2demo'
        $state.multiplayer.mission_title = 'Descent 2 Demo'
    }
    return $state
}
function Test-Peer($State) {
    Test-PublicLanPeerState -State $State -LocalCallsign 'PrepH1' -RemoteCallsign 'PrepJ1' -IsHost $true -PigSize 5092871 -NativeLog 'Loaded native library: dxx-redux-d2'
}
if (-not (Test-Peer (New-PeerState))) { throw 'Valid public host state was rejected' }
$client = New-PeerState
$client.multiplayer.i_am_master = $false
$client.multiplayer.players[0].is_me = $false
$client.multiplayer.players[1].is_me = $true
if (-not (Test-PublicLanPeerState -State $client -LocalCallsign 'PrepJ1' -RemoteCallsign 'PrepH1' -IsHost $false -PigSize 5092871 -NativeLog 'Loaded native library: dxx-redux-d2')) {
    throw 'Valid public client state was rejected'
}
foreach ($missing in @($null, @{}, @{ pid = 1234 }, @{ in_game = $true; is_network = $true })) {
    if (Test-Peer $missing) { throw 'Missing content or process-only evidence passed' }
}
$mutations = @(
    { param($s) $s.in_game = $false },
    { param($s) $s.is_network = $false },
    { param($s) $s.screen_mode = 'menu' },
    { param($s) $s.current_level_num = 2 },
    { param($s) $s.asset_trace.runtime_engine = 'd1' },
    { param($s) $s.asset_trace.selected_base_game = 'd2' },
    { param($s) $s.asset_trace.mode = 'd2' },
    { param($s) $s.asset_trace.d1_compat.native_assets = $false },
    { param($s) $s.asset_trace.d1_compat.sound_pig_size = 4920305 },
    { param($s) $s.asset_trace.d1_compat.sound_files = 98 },
    { param($s) $s.multiplayer.mission_name = 'd2demo' },
    { param($s) $s.multiplayer.mission_title = 'Descent: First Strike' },
    { param($s) $s.multiplayer.level_num = 2 },
    { param($s) $s.multiplayer.gamemode_name = 'anarchy' },
    { param($s) $s.multiplayer.num_players = 1 },
    { param($s) $s.multiplayer.num_connected = 1 },
    { param($s) $s.multiplayer.num_connected = 3 },
    { param($s) $s.multiplayer.i_am_master = $false },
    { param($s) $s.multiplayer.Remove('i_am_master') },
    { param($s) $s.multiplayer.players = @($s.multiplayer.players[0]) },
    { param($s) $s.multiplayer.players[0].is_me = $false },
    { param($s) $s.multiplayer.players[1].is_me = $true },
    { param($s) $s.multiplayer.players[0].callsign = 'preph2' },
    { param($s) $s.multiplayer.players[1].callsign = 'prepj2' },
    { param($s) $s.multiplayer.players[0].connected = 0 },
    { param($s) $s.multiplayer.players[1].connected = 0 }
)
foreach ($mutation in $mutations) {
    $state = New-PeerState
    & $mutation $state
    if (Test-Peer $state) { throw "Accepted invalid public LAN evidence: $mutation" }
}

foreach ($case in @(@{ Engine = 'd1'; Content = 'd1' }, @{ Engine = 'd2'; Content = 'd2' }, @{ Engine = 'd2'; Content = 'd1' })) {
    foreach ($isHost in @($true, $false)) {
        $state = New-PeerState -Engine $case.Engine -Content $case.Content
        $state.multiplayer.i_am_master = $isHost
        $parameters = @{
            LocalCallsign = 'PrepH1'; RemoteCallsign = 'PrepJ1'; IsHost = $isHost; PigSize = 5092871
            Engine = $case.Engine; Content = $case.Content; NativeLog = "Loaded native library: dxx-redux-$($case.Engine)"
        }
        if (-not (Test-PublicLanPeerState -State $state @parameters)) { throw "Valid public $($case.Engine)/$($case.Content) state was rejected" }
        foreach ($badLog in @('', 'Process started', 'Loaded native library: dxx-redux-d1x', "Loaded native library: dxx-redux-d1`nLoaded native library: dxx-redux-d2")) {
            if (Test-PublicLanEngineLog -Log $badLog -Engine $case.Engine) { throw 'Missing or conflicting native engine identity passed' }
        }
        $otherEngine = if ($case.Engine -eq 'd1') { 'd2' } else { 'd1' }
        $parameters.NativeLog = "Loaded native library: dxx-redux-$otherEngine"
        if (Test-PublicLanPeerState -State $state @parameters) { throw 'Wrong native engine passed with otherwise valid state' }
        $parameters.NativeLog = "Loaded native library: dxx-redux-$($case.Engine)"
        $state.multiplayer.players[1].connected = 0
        if (Test-PublicLanPeerState -State $state @parameters) { throw 'Disconnected remote player passed a native or imported case' }
        $state.multiplayer.players[1].connected = 1
        $state.multiplayer.mission_name = 'wrong'
        if (Test-PublicLanPeerState -State $state @parameters) { throw 'Wrong mission passed a native or imported case' }
    }
}
$nativeD1 = New-PeerState -Engine d1 -Content d1
$nativeD1.asset_trace = (New-PeerState).asset_trace
if (Test-PublicLanPeerState -State $nativeD1 -LocalCallsign 'PrepH1' -RemoteCallsign 'PrepJ1' -IsHost $true -PigSize 5092871 -Engine d1 -Content d1 -NativeLog 'Loaded native library: dxx-redux-d1') {
    throw 'D2 introspection accepted as native D1'
}
# The device returns PSCustomObject JSON, whose absent properties throw under the master's StrictMode
& {
    Set-StrictMode -Version Latest
    foreach ($isHost in @($true, $false)) {
        $state = New-PeerState -Engine d1 -Content d1 | ConvertTo-Json -Depth 10 | ConvertFrom-Json
        if ($null -ne $state.PSObject.Properties['asset_trace']) { throw 'Native D1 JSON fixture must omit asset_trace' }
        $state.multiplayer.i_am_master = $isHost
        $state.multiplayer.players[0].is_me = $isHost
        $state.multiplayer.players[1].is_me = -not $isHost
        $parameters = @{
            LocalCallsign = $(if ($isHost) { 'PrepH1' } else { 'PrepJ1' })
            RemoteCallsign = $(if ($isHost) { 'PrepJ1' } else { 'PrepH1' })
            IsHost = $isHost; PigSize = 5092871; Engine = 'd1'; Content = 'd1'
            NativeLog = 'Loaded native library: dxx-redux-d1'
        }
        if (-not (Test-PublicLanPeerState -State $state @parameters)) { throw 'Native D1 JSON without asset_trace failed under StrictMode' }
        $state | Add-Member -NotePropertyName asset_trace -NotePropertyValue (New-PeerState).asset_trace
        if (Test-PublicLanPeerState -State $state @parameters) { throw 'D2 JSON trace accepted as native D1 under StrictMode' }
        $state.asset_trace = @()
        $state = $state | ConvertTo-Json -Depth 10 | ConvertFrom-Json
        if (Test-PublicLanPeerState -State $state @parameters) { throw 'Non-null empty JSON trace accepted as native D1 under StrictMode' }
    }
}

function Assert-Rejected([string]$Label, [scriptblock]$Action) {
    try { & $Action } catch { Write-Host "PASS: rejects $Label"; return }
    throw "Accepted invalid public LAN fixture: $Label"
}
$script:requests = [Collections.Generic.List[object]]::new()
$script:offline = ''
$script:physical = ''
$script:badClear = $false
$script:badHash = $false
$script:badLobbyClear = $false
$script:publishedLobby = ''
$script:nextLobby = ''
$script:PACKAGE = 'com.dxxredux.test'
$script:DEFAULT_SET_DIR = 'files/imported/sets/default'
$D1DataDirectory = $PSScriptRoot
$publicFiles = @(foreach ($name in @('descent.hog', 'descent.pig', 'd2demo.hog', 'd2demo.ham', 'd2demo.pig', 'd2demo.dem')) {
        @{ File = $name; Sha256 = ('a' * 64); Source = (Join-Path $PSScriptRoot $name) }
    })
function Adb-Dev-Timeout {
    param([string]$Serial, [string[]]$AdbArgs, [int]$Seconds)
    $script:requests.Add(@{ Serial = $Serial; Arguments = $AdbArgs })
    if ($AdbArgs[0] -eq 'get-state') { if ($Serial -eq $script:offline) { return $null }; return 'device' }
    if ($AdbArgs -contains 'getprop') { if ($Serial -eq $script:physical) { return '0' }; return '1' }
    if ($AdbArgs -contains 'clear') { if ($script:badClear) { return $null }; return 'Success' }
    if ($AdbArgs[-1] -like '*rm -f files/mp_introspect.json*') {
        if ($script:badLobbyClear) { return $null }
        $script:publishedLobby = ''
        return 'DXX_LAN_STATE_CLEARED'
    }
    if ($AdbArgs -contains 'cat' -and $AdbArgs[-1] -ceq 'files/mp_introspect.json') { return $script:publishedLobby }
    if ($AdbArgs -contains 'sha256sum') {
        $path = $AdbArgs[-1]
        $file = @($publicFiles | Where-Object { $path.EndsWith('/' + $_.File) })[0]
        if ($script:badHash) { return "wrong  $path" }
        return "$($file.Sha256)  $path"
    }
}
function Send-MpCommand {
    param([string]$Serial, [string]$Command)
    if ($Command -cne 'introspect') { throw 'Unexpected mocked lobby command' }
    $script:requests.Add(@{ Serial = $Serial; Arguments = @('MP_COMMAND', $Command) })
    $script:publishedLobby = $script:nextLobby
}
Assert-LanPreparationTargets -HostSerial 'emulator-5554' -ClientSerial 'emulator-5556'
if ($script:requests.Count -ne 4 -or @($script:requests | Where-Object { $_.Arguments[0] -notin @('get-state', 'shell') }).Count) {
    throw 'Target admission must only inspect the configured pair'
}
$script:requests.Clear()
Assert-Rejected 'physical host' { Assert-LanPreparationTargets -HostSerial 'phone123' -ClientSerial 'emulator-5556' }
Assert-Rejected 'physical client' { Assert-LanPreparationTargets -HostSerial 'emulator-5554' -ClientSerial 'phone123' }
Assert-Rejected 'duplicate targets' { Assert-LanPreparationTargets -HostSerial 'emulator-5554' -ClientSerial 'emulator-5554' }
if ($script:requests.Count) { throw 'Invalid serials reached adb' }
$script:physical = 'emulator-5556'
Assert-Rejected 'serial without emulator property' { Assert-LanPreparationTargets -HostSerial 'emulator-5554' -ClientSerial 'emulator-5556' }
$script:physical = ''
$script:offline = 'emulator-5556'
Assert-Rejected 'offline client' { Assert-LanPreparationTargets -HostSerial 'emulator-5554' -ClientSerial 'emulator-5556' }
$script:offline = ''
$script:requests.Clear()
Stage-PublicLanAssets -Serial 'emulator-5554'
if (@($script:requests | Where-Object { $_.Arguments[0] -eq 'push' }).Count -ne 6 -or
    @($script:requests | Where-Object { $_.Arguments -contains 'sha256sum' }).Count -ne 6) {
    throw 'Public staging must copy and hash-check all six pinned D1/D2 files'
}
$script:requests.Clear()
Stage-PublicLanAssets -Serial 'emulator-5554' -Files @($publicFiles[0], $publicFiles[1])
if (@($script:requests | Where-Object { $_.Arguments[0] -eq 'push' }).Count -ne 2 -or
    @($script:requests | Where-Object { $_.Arguments -contains 'sha256sum' }).Count -ne 2) {
    throw 'The D1-only candidate must stage and verify only its two files'
}
$script:requests.Clear()
$script:badClear = $true
Assert-Rejected 'failed app reset' { Stage-PublicLanAssets -Serial 'emulator-5554' }
if ($script:requests.Count -ne 1) { throw 'Failed reset continued to stage assets' }
$script:badClear = $false
$script:badHash = $true
Assert-Rejected 'changed staged public asset' { Assert-PublicLanDeviceAssets -Serial 'emulator-5554' }

& {
    Set-StrictMode -Version Latest
    $hostOnly = '{"callsign":"PrepH3","lan":{"hosting":true,"players":[{"callsign":"PrepH3","connected":true,"ready":true}]}}'
    $twoNotReady = '{"callsign":"PrepH3","lan":{"hosting":true,"players":[{"callsign":"PrepH3","connected":true,"ready":true},{"callsign":"PrepJ3","connected":true,"ready":false}]}}'
    $twoReady = $twoNotReady.Replace('"ready":false', '"ready":true')
    $peers = @{ HostCallsign = 'PrepH3'; ClientCallsign = 'PrepJ3' }
    if (Test-LanPreparationLobbyPeers -State ($hostOnly | ConvertFrom-Json) @peers -RequireReady) { throw 'Host-only ready lobby passed' }
    # Reproduce stage7: earlier host-only ready evidence must not override two unready peers
    $script:publishedLobby = $hostOnly
    $script:nextLobby = $twoNotReady
    $script:requests.Clear()
    $state = Read-LanPreparationLobby -Serial 'emulator-5554'
    if (-not (Test-LanPreparationLobbyPeers -State $state @peers)) { throw 'Connected two-player lobby was not joined' }
    if (Test-LanPreparationLobbyPeers -State $state @peers -RequireReady) { throw 'Older host-only readiness overrode the current unready client' }
    if ($script:requests.Count -ne 3 -or $script:requests[0].Arguments[-1] -notlike '*rm -f files/mp_introspect.json*' -or
        $script:requests[1].Arguments[0] -cne 'MP_COMMAND' -or $script:requests[2].Arguments[-1] -cne 'files/mp_introspect.json') {
        throw 'Lobby evidence must be invalidated, requested, then read'
    }
    $script:nextLobby = $twoReady
    $state = Read-LanPreparationLobby -Serial 'emulator-5554'
    if (-not (Test-LanPreparationLobbyPeers -State $state @peers -RequireReady)) { throw 'Current two-player ready lobby was rejected' }
    foreach ($mutation in @(
            { param($s) $s.callsign = 'OtherHost' },
            { param($s) $s.lan.hosting = $false },
            { param($s) $s.lan.players[1].callsign = 'PrepJ2' },
            { param($s) $s.lan.players[1].callsign = 'PrepH3' },
            { param($s) $s.lan.players[1].connected = $false },
            { param($s) $s.lan.players[0].ready = $false },
            { param($s) $s.lan.players[1].PSObject.Properties.Remove('ready') }
        )) {
        $state = $twoReady | ConvertFrom-Json
        & $mutation $state
        if (Test-LanPreparationLobbyPeers -State $state @peers -RequireReady) { throw 'Wrong, disconnected or unready session peers passed' }
    }
    foreach ($missing in @($null, ('{}' | ConvertFrom-Json), ('{"callsign":"PrepH3","lan":null}' | ConvertFrom-Json))) {
        if (Test-LanPreparationLobbyPeers -State $missing @peers -RequireReady) { throw 'Missing lobby state passed' }
    }
    foreach ($raw in @('', '{')) {
        $script:nextLobby = $raw
        if ($null -ne (Read-LanPreparationLobby -Serial 'emulator-5554')) { throw 'Unpublished or malformed current state reused prior ready evidence' }
    }
    $script:publishedLobby = $twoReady
    $script:badLobbyClear = $true
    $script:requests.Clear()
    Assert-Rejected 'failed lobby evidence invalidation' { Read-LanPreparationLobby -Serial 'emulator-5554' }
    if ($script:requests.Count -ne 1) { throw 'Failed invalidation continued to request or read stale lobby evidence' }
    $script:badLobbyClear = $false
}

# These are orchestration controls, not claims that an emulator session ran
$source = Get-Content -LiteralPath $runnerPath -Raw
foreach ($required in @(
        'Get-GameIntrospection -Serial $HostSerial -Fresh',
        '$state = Read-LanPreparationLobby -Serial $HostSerial',
        'Test-LanPreparationLobbyPeers -State $state -HostCallsign $hostCallsign -ClientCallsign $clientCallsign -RequireReady',
        'Send-MpCommand -Serial $HostSerial -Command ''lan_host_lobby'' -Extras $hostExtras',
        'LAN final peer evidence:',
        'logcat -c && echo DXX_LAN_LOG_CLEARED',
        'rm -f files/mp_introspect.json && echo DXX_LAN_STATE_CLEARED',
        'gameplay_tested = $false',
        'if ($cleared -cne ''DXX_LAN_LOG_CLEARED'')',
        'Get-GameIntrospection -Serial $ClientSerial -Fresh',
        'Resolve-SharewareTestData -RepoRoot $script:REPO_ROOT -Game d1 -DataDir $D1DataDirectory',
        'Resolve-SharewareTestData -RepoRoot $script:REPO_ROOT -Game d2 -DataDir $D2DataDirectory',
        'Invoke-LanPreparationSession -HostSerial $hostSerial -ClientSerial $clientSerial -Session 1 -Engine d1 -Content d1 -SessionMission ''''',
        'Invoke-LanPreparationSession -HostSerial $clientSerial -ClientSerial $hostSerial -Session 2 -Engine d2 -Content d2 -SessionMission ''d2demo''',
        'Invoke-LanPreparationSession -HostSerial $hostSerial -ClientSerial $clientSerial -Session 3 -Engine d2 -Content d1 -SessionMission ''descent''',
        '-Engine $Engine -Content $Content -NativeLog $hostNativeLog',
        '-Engine $Engine -Content $Content -NativeLog $clientNativeLog'
    )) {
    if (-not $source.Contains($required)) { throw "Public LAN orchestration control missing: $required" }
}
if ($source.Contains("-match 'players=2'") -or $source.Contains("-match 'all_ready=true'")) {
    throw 'Historical status lines must not prove current lobby membership or readiness'
}
$guard = $source.IndexOf('Assert-LanPreparationTargets -HostSerial $hostSerial -ClientSerial $clientSerial')
$stage = $source.IndexOf('foreach ($serial in @($hostSerial, $clientSerial)) { Stage-PublicLanAssets -Serial $serial }')
$candidate = $source.LastIndexOf('Invoke-D1OnlyLanDiagnostic')
$nativeD1Start = $source.IndexOf('Invoke-LanPreparationSession -HostSerial $hostSerial -ClientSerial $clientSerial -Session 1 -Engine d1')
$nativeD2Start = $source.IndexOf('Invoke-LanPreparationSession -HostSerial $clientSerial -ClientSerial $hostSerial -Session 2 -Engine d2 -Content d2')
$importedStart = $source.IndexOf('Invoke-LanPreparationSession -HostSerial $hostSerial -ClientSerial $clientSerial -Session 3 -Engine d2 -Content d1')
if ($guard -lt 0 -or $candidate -lt $guard -or $nativeD1Start -lt $candidate -or $stage -lt $nativeD1Start -or
    $nativeD2Start -lt $stage -or $importedStart -lt $nativeD2Start) {
    throw 'Admission and native D1 must precede D2 staging; native starts must precede imported content'
}
$recoverySource = Join-Path $PSScriptRoot '../helpers/check_lan_parallel_startup.ps1'
$recoveryAst = [Management.Automation.Language.Parser]::ParseFile($recoverySource, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Recovery helper parser errors: $parseErrors" }
$recoveryPredicate = $recoveryAst.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Test-LanRecoveryMenu'
    }, $true)
if (-not $recoveryPredicate) { throw 'Recovery menu predicate missing' }
. ([scriptblock]::Create($recoveryPredicate.Extent.Text))
# The real synchronization dialog already has screen_mode=menu and in_game=false
$waiting = @{
    in_game = $false; screen_mode = 'menu'; is_network = $true; game_window_is_front = $false
    menu = @{ type = 'newmenu'; title = ''; subtitle = 'Wait...'; items = @(@{ text = 'Waiting for host to enter' }) }
}
$message = 'The host did not finish starting within 60 seconds'
if (Test-LanRecoveryMenu $waiting $message) { throw 'Synchronization Wait dialog passed as recovery' }
if (Test-LanRecoveryMenu $null $message) { throw 'Absent observation passed as recovery' }
$errorDialog = $waiting.Clone()
$errorDialog.menu = @{ type = 'newmenu'; title = 'Error'; subtitle = $message; items = @(@{ text = 'OK' }) }
if (-not (Test-LanRecoveryMenu $errorDialog $message)) { throw 'Explicit expected failure dialog was rejected' }
if (Test-LanRecoveryMenu $errorDialog 'Different failure') { throw 'Unrelated dialog passed as expected recovery' }
$mainMenu = $waiting.Clone()
$mainMenu.is_network = $false
$mainMenu.menu = @{ type = 'newmenu'; title = ''; subtitle = ''; items = @(@{ text = 'New game' }, @{ text = 'Options...' }, @{ text = 'Quit' }) }
if (-not (Test-LanRecoveryMenu $mainMenu)) { throw 'Disconnected main menu was rejected' }
$mainMenu.is_network = $true
if (Test-LanRecoveryMenu $mainMenu) { throw 'Network-active menu passed as disconnected main menu' }
$errorDialog.game_window_is_front = $true
if (Test-LanRecoveryMenu $errorDialog $message) { throw 'Foreground game window passed as recovery dialog' }
# Three-player identity controls reject a duplicated, dropped or unrelated peer
$threePlayerHelper = Join-Path $PSScriptRoot '../helpers/check_lan_three_player_startup.ps1'
$threeAst = [Management.Automation.Language.Parser]::ParseFile($threePlayerHelper, [ref]$null, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Three-player helper parser errors: $parseErrors" }
$threePredicate = $threeAst.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Test-LanThreePlayerState'
    }, $true)
. ([scriptblock]::Create($threePredicate.Extent.Text))
$threeState = @{
    is_network = $true
    multiplayer = @{
        num_players = 3; num_connected = 3; level_num = 1; gamemode_name = 'cooperative'
        players = @(
            @{ callsign = 'TimingH'; is_me = $true; connected = 1 },
            @{ callsign = 'TimingJ'; is_me = $false; connected = 1 },
            @{ callsign = 'TimingK'; is_me = $false; connected = 1 }
        )
    }
}
if (-not (Test-LanThreePlayerState $threeState)) { throw 'Three distinct connected peers were rejected' }
foreach ($field in @('num_players', 'num_connected')) {
    $threeState.multiplayer[$field] = 2
    if (Test-LanThreePlayerState $threeState) { throw "Incomplete $field passed three-player coverage" }
    $threeState.multiplayer[$field] = 3
}
$threeState.multiplayer.players[2].callsign = 'TimingJ'
if (Test-LanThreePlayerState $threeState) { throw 'Duplicate callsign passed three-player coverage' }
$threeState.multiplayer.players[2].callsign = 'TimingK'
$threeState.multiplayer.players[2].connected = 0
if (Test-LanThreePlayerState $threeState) { throw 'Disconnected peer passed three-player coverage' }
$threeState.multiplayer.players[2].connected = 1
$threeState.multiplayer.players[1].is_me = $true
if (Test-LanThreePlayerState $threeState) { throw 'Duplicate local identity passed three-player coverage' }

# A caller-owned emulator must fail a reset without entering generic restart recovery
& {
    $helperAst = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot '../helpers/test_helpers.ps1'), [ref]$null, [ref]$parseErrors)
    if ($parseErrors.Count) { throw "Test helper parser errors: $parseErrors" }
    foreach ($name in @('Reset-GameState', 'Reset-DeviceGameState')) {
        $definition = $helperAst.Find({ param($node)
                $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $name
            }, $true)
        . ([scriptblock]::Create($definition.Extent.Text))
    }
    $resetProbe = @{ Recoveries = 0 }
    function Adb { param($AdbArgs) }
    function Stop-AppAndWait { }
    function Write-Status { param($Message, $Color) }
    function Publish-DefaultActiveFileSet { return $false }
    function Ensure-EmulatorHealthy { $resetProbe.Recoveries++ }
    $previousSerial = $env:ANDROID_SERIAL
    foreach ($noRecovery in @($true, $false)) {
        $caught = $false
        try { Reset-DeviceGameState -Serial 'emulator-5558' -NoEmulatorRecovery:$noRecovery } catch {
            $expected = if ($noRecovery) { 'Default active game-data publication failed; caller-owned emulator was not recycled' } else { 'could not publish the default active game-data set after emulator recovery' }
            if ($_.Exception.Message -cne $expected) { throw }
            $caught = $true
        }
        if (-not $caught -or $resetProbe.Recoveries -ne [int](-not $noRecovery) -or $env:ANDROID_SERIAL -cne $previousSerial) {
            throw 'Reset recovery boundary or device environment restoration changed'
        }
    }
}

# Execute the real controller against deterministic host-only peers, never adb
& {
    . $threePlayerHelper
    $previousThird = $env:DXX_THIRD_EMULATOR_SERIAL
    $env:DXX_THIRD_EMULATOR_SERIAL = 'emulator-5558'
    $hostSerial = 'emulator-5554'
    $clientSerial = 'emulator-5556'
    $script:REPO_ROOT = 'unused-host-fixture'
    $script:PACKAGE = 'com.example.fixture'
    $script:latencyClock = [Diagnostics.Stopwatch]::StartNew()
    $publicFiles = @(@{ Game = 'd1' }, @{ Game = 'd2' })
    $fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('three-player-controller-' + [guid]::NewGuid().ToString('N'))
    function git { $global:LASTEXITCODE = 0; return ('a' * 40) }
    function Stage-PublicLanAssets { param($Serial, $Files) $script:stageCount++ }
    function Assert-PublicLanDeviceAssets { param($Serial, $Files) }
    function Reset-DeviceGameState { param($Serial, [switch]$NoEmulatorRecovery)
        if ([bool]$NoEmulatorRecovery -ne ($Serial -ceq 'emulator-5558')) { throw 'Caller-owned third emulator must not enter reset recovery' }
    }
    function Write-LatencyEvent { param($Peer, $Event, $Data) }
    function Add-ControllerEvent {
        param($Serial, $Event)
        $script:controllerEvents.Add(@{ Case = "$script:latencyGame/$script:latencySample"; Serial = $Serial; Event = $Event })
    }
    function Invoke-LatencyCommand {
        param($Serial, $Command, $Extras)
        if ($Command -in @('lan_start_game', 'launch_game')) { Add-ControllerEvent $Serial $Command }
        if ($Command -ceq 'launch_game') {
            $mode = if ($Serial -ceq 'emulator-5558') { $script:gameLogFixture } else { 'correct' }
            if ($mode -notin @('missing', 'wrong')) {
                $script:peerLogs[$Serial].Add("  101.000  2000  2000 I MainActivity: Loaded native library: dxx-redux-$script:latencyGame")
            }
            if ($mode -in @('wrong', 'conflicting')) {
                $otherEngine = if ($script:latencyGame -ceq 'd1') { 'd2' } else { 'd1' }
                $script:peerLogs[$Serial].Add("  101.001  2000  2000 I MainActivity: Loaded native library: dxx-redux-$otherEngine")
            }
            foreach ($event in @('wait_requests_flush', 'load_hold_begin', 'initial_load_peer_lost', 'initial_sync_timeout')) {
                $script:peerLogs[$Serial].Add("event=$event")
            }
        }
    }
    function Invoke-LatencyAutomation { param($Phase) }
    function Save-LatencyAutomation { param($Phase) }
    function Assert-LanRecoveryMenu { param($Serial, $Event, $ErrorText) }
    function Test-EmulatorHealthy { param($Serial) return $true }
    function Start-SetupActivity {
        param($Serial)
        Add-ControllerEvent $Serial 'setup'
        # Launcher services load D2 even when the selected game is native D1
        $script:peerLogs[$Serial].Add('  100.000  1000  1000 I SetupActivity: Loaded native library: dxx-redux-d2')
        return $true
    }
    function Send-MpCommand { param($Serial, $Command, $Extras) }
    function Read-LanPreparationLobby {
        param($Serial)
        return @{ lan = @{ players = @(
                    @{ callsign = 'TimingH'; connected = $true; ready = $true },
                    @{ callsign = 'TimingJ'; connected = $true; ready = $true },
                    @{ callsign = 'TimingK'; connected = $true; ready = $true }
                ) } }
    }
    function Adb-Dev-Timeout {
        param($Serial, $AdbArgs, $Seconds)
        if ($AdbArgs[0] -ceq 'get-state') { return 'device' }
        if ($AdbArgs -contains 'getprop') { return '1' }
        if ($AdbArgs[0] -ceq 'logcat') {
            if ($AdbArgs -contains '-c') {
                Add-ControllerEvent $Serial 'logcat-clear'
                $script:peerLogs[$Serial].Clear()
                return ''
            }
            if ($AdbArgs -notcontains '-d') { throw 'Unexpected mocked logcat command' }
            return ($script:peerLogs[$Serial] -join "`n")
        }
        if ($AdbArgs -contains 'pidof') { return '12345' }
        if ($AdbArgs -contains 'sh') { return 'STOPPED' }
        return ''
    }
    function Wait-ForCondition {
        param($Description, $TimeoutSec, $PollMs, $Condition)
        $ready = & $Condition
        if ($ready -and $Description -ceq 'All three launcher players ready') {
            Add-ControllerEvent 'all' 'ready'
            foreach ($serial in @($hostSerial, $clientSerial, 'emulator-5558')) {
                $script:peerLogs[$serial].Add('  100.500  1000  1000 I SetupActivity: event=launcher_ready')
            }
        }
        return $ready
    }
    function Wait-LatencyPeers {
        param($Event, $Predicate, $Timeout)
        if ($script:failFirst -and $script:latencyGame -ceq 'd1' -and $Event -ceq 'three.briefing.ready') {
            throw 'Intentional host-only first-case failure'
        }
    }
    try {
        $expected = @('d1/three-player', 'd1/three-player-lost', 'd2/three-player', 'd2/three-player-lost')
        foreach ($fixture in @('correct', 'first-case-failure', 'missing', 'wrong', 'conflicting')) {
            $failure = $fixture -cne 'correct'
            $script:failFirst = $fixture -ceq 'first-case-failure'
            $script:gameLogFixture = $fixture
            $expectedFailures = @(if ($script:failFirst) { 'd1/three-player' } elseif ($failure) { $expected })
            $script:stageCount = 0
            $script:controllerEvents = [Collections.Generic.List[object]]::new()
            $script:peerLogs = @{}
            foreach ($serial in @($hostSerial, $clientSerial, 'emulator-5558')) {
                $script:peerLogs[$serial] = [Collections.Generic.List[string]]::new()
            }
            $outputDirectory = Join-Path $fixtureRoot $fixture
            New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
            $caught = $false
            try { Invoke-LanThreePlayerStartupChecks } catch {
                if (-not $failure -or $_.Exception.Message -cne 'Three-player startup category did not pass every selected case') { throw }
                $caught = $true
            }
            if ($caught -ne $failure) { throw 'Controller did not propagate the case failure' }
            $summary = Get-Content (Join-Path $outputDirectory 'three-player-summary.json') -Raw | ConvertFrom-Json
            if ($summary.results.Count -ne 4 -or -not $summary.coverage_complete -or $summary.passed -eq $failure -or
                $summary.revision -cne ('a' * 40) -or $summary.category -cne 'three_player_network') {
                throw 'Controller summary did not retain four independent terminal results'
            }
            $failedCases = @($summary.results | Where-Object status -EQ 'FAIL')
            if (($summary.results.id -join ',') -cne ($expected -join ',') -or
                (@($failedCases | ForEach-Object id) -join ',') -cne ($expectedFailures -join ',') -or $script:stageCount -ne 6) {
                throw 'Controller repeated asset staging or omitted cases after a failure'
            }
            foreach ($failed in $failedCases) {
                $reason = if ($script:failFirst) { 'Intentional host-only first-case failure' }
                else { "Expected native $($failed.id.Split('/')[0]) engine was not observed on emulator-5558" }
                if ($failed.reason -cne $reason) { throw 'Controller failed for an unexpected reason' }
            }
            $expectedEvents = @(
                foreach ($serial in @($hostSerial, $clientSerial, 'emulator-5558')) { "$serial/logcat-clear"; "$serial/setup" }
                'all/ready'
                "$hostSerial/lan_start_game"
                foreach ($serial in @($hostSerial, $clientSerial, 'emulator-5558')) { "$serial/launch_game" }
            )
            foreach ($case in $expected) {
                $events = @($script:controllerEvents | Where-Object Case -CEQ $case | ForEach-Object { "$($_.Serial)/$($_.Event)" })
                if (($events -join ',') -cne ($expectedEvents -join ',')) { throw "Fresh case capture must retain launcher and game evidence: $case" }
                $casePath = Join-Path $outputDirectory ('parallel-startup-checks/' + $case.Replace('/', '-') + '/case-result.json')
                if (-not (Test-Path -LiteralPath $casePath)) { throw "Missing independent case result: $case" }
                $leaf = Get-Content -LiteralPath $casePath -Raw | ConvertFrom-Json
                $result = @($summary.results | Where-Object id -CEQ $case)[0]
                if ($leaf.id -cne $case -or $leaf.status -cne $result.status -or $leaf.reason -cne $result.reason) {
                    throw "Independent case result disagrees with the summary: $case"
                }
                foreach ($serial in @($hostSerial, $clientSerial, 'emulator-5558')) {
                    $logPath = Join-Path (Split-Path -Parent $casePath) ("$($case.Replace('/', '-'))-$serial-logcat.txt")
                    $rawLog = Get-Content -LiteralPath $logPath -Raw
                    if (-not $rawLog.Contains('I SetupActivity: Loaded native library: dxx-redux-d2') -or
                        -not $rawLog.Contains('I SetupActivity: event=launcher_ready')) {
                        throw "Native-engine filtering discarded raw launcher evidence: $case $serial"
                    }
                }
            }
        }
        $env:DXX_THIRD_EMULATOR_SERIAL = $clientSerial
        $script:stageCount = 0
        $caught = $false
        try { Invoke-LanThreePlayerStartupChecks } catch {
            if ($_.Exception.Message -cne 'Three distinct disposable emulators are required') { throw }
            $caught = $true
        }
        if (-not $caught -or $script:stageCount -ne 0) { throw 'Aliased third emulator reached destructive fixture setup' }
    } finally {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
        if ($null -eq $previousThird) { Remove-Item Env:DXX_THIRD_EMULATOR_SERIAL -ErrorAction SilentlyContinue }
        else { $env:DXX_THIRD_EMULATOR_SERIAL = $previousThird }
    }
}
$threeOwner = Get-Content (Join-Path $PSScriptRoot 'test_lan_three_player_startup.ps1') -Raw
if ($threeOwner -notmatch '-PublicShareware -ThreePlayerStartup -RecoveryChecks:\$false -MeasureTransitions:\$false') {
    throw 'Independent owner must explicitly disable inherited experiment switches'
}
Write-Host 'PASS: public LAN and independent three-player evidence, safe setup reuse, complete receipts and negative controls'
