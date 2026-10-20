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
$mainMenu.menu = @{ type = 'newmenu'; title = ''; subtitle = ''; items = @(@{ text = 'New game' }, @{ text = 'Options' }, @{ text = 'Quit' }) }
if (-not (Test-LanRecoveryMenu $mainMenu)) { throw 'Disconnected main menu was rejected' }
$mainMenu.is_network = $true
if (Test-LanRecoveryMenu $mainMenu) { throw 'Network-active menu passed as disconnected main menu' }
$errorDialog.game_window_is_front = $true
if (Test-LanRecoveryMenu $errorDialog $message) { throw 'Foreground game window passed as recovery dialog' }
Write-Host 'PASS: public LAN state rejects false success, physical targets, altered assets and nonterminal waiting menus; retail remains default'
