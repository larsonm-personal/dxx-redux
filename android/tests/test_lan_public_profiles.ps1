#!/usr/bin/env pwsh
# Device-free negative controls for the bounded native-D1 public migration owner.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$errors = $null
$owner = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'test_lan.ps1'), [ref]$null, [ref]$errors)
if ($errors.Count) { throw "LAN owner parse errors: $errors" }
foreach ($name in @('Start-LanLogcatCapture', 'Assert-PublicMigrationArguments', 'Assert-PublicMigrationTargets', 'Stage-PublicMigrationAssets',
        'Test-PublicMigrationLobby', 'Test-PublicMigrationState', 'Test-PublicMigrationEngineLog', 'Cleanup', 'Test-IntroHostRole', 'Get-IntroMultiplayer', 'Get-IntroPdataSequence', 'Wait-BidirectionalPdata')) {
    $definition = $owner.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $true)
    if (-not $definition) { throw "Missing owner function $name" }
    . ([scriptblock]::Create($definition.Extent.Text))
}
# Exercise the exact production launcher under both host-platform branches.
$ADB = 'fixture-adb'
function Test-RegressionWindowsHost { return $script:windowsCaptureHost }
function Start-Process {
    param($FilePath, $ArgumentList, [switch]$PassThru, $RedirectStandardOutput,
        $RedirectStandardError, $WindowStyle)
    $script:captureOptions = $PSBoundParameters
    return [pscustomobject]@{ Id = 123 }
}
foreach ($windows in @($false, $true)) {
    $script:windowsCaptureHost = $windows
    foreach ($matchmaking in @($false, $true)) {
        $process = Start-LanLogcatCapture -Serial emulator-5554 -OutputPath capture.log -ErrorPath capture.err -IncludeMatchmaking:$matchmaking
        $actual = $script:captureOptions
        if ($process.Id -ne 123 -or $actual.FilePath -cne 'fixture-adb' -or -not $actual.PassThru -or
            $actual.RedirectStandardOutput -cne 'capture.log' -or $actual.RedirectStandardError -cne 'capture.err') {
            throw 'Logcat capture lost process ownership or redirected evidence'
        }
        if ($actual.ContainsKey('WindowStyle') -ne $windows -or ($windows -and $actual.WindowStyle -cne 'Hidden')) {
            throw 'Logcat capture WindowStyle is not Windows-only'
        }
        $expected = @('-s', 'emulator-5554', 'logcat', '-s', 'DXX-MP:*', 'DXX-Redux:*',
            'DXX-DLOG:*', 'DXX-Automate:*', 'dxxredux:*', 'AndroidRuntime:*', 'LocalhostProxy:*')
        if ($matchmaking) { $expected += 'MatchmakingService:*' }
        if (($actual.ArgumentList -join '|') -cne ($expected -join '|')) { throw 'Logcat capture filters changed' }
    }
}
Remove-Item Function:Start-Process
$public = $owner.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -eq 'PublicShareware' }
$game = $owner.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -eq 'Game' }
if (-not $public -or $public.DefaultValue -or $game.DefaultValue.SafeGetValue() -cne 'd2') { throw 'Retail defaults changed' }
function Assert-Rejected([scriptblock]$Action) {
    $rejected = $false
    try { & $Action } catch { $rejected = $true }
    if (-not $rejected) { throw 'Invalid public profile was admitted' }
}
$profile = @{ Arguments = @{ PublicShareware = $true; Game = 'd1'; HostMigration = $true }; SelectedGame = 'd1'
    Migration = $true; Mission = ''; Level = 1; HostName = 'LanHost'; JoinName = 'LanJoin' }
Assert-PublicMigrationArguments @profile
foreach ($change in @(
        @{ SelectedGame = 'd2' }, @{ Migration = $false }, @{ Mission = 'descent' }, @{ Level = 2 },
        @{ HostName = 'lanjoin' }, @{ JoinName = 'LongCallsign' }, @{ Arguments = @{ PublicShareware = $true; UseRelay = $false } }
    )) {
    $bad = $profile.Clone()
    foreach ($key in $change.Keys) { $bad[$key] = $change[$key] }
    Assert-Rejected { Assert-PublicMigrationArguments @bad }
}
function New-MigrationState([int]$Slot = 0, [int]$Master = 0) {
    # Serialize and deserialize: native D1 omits asset_trace and emits the active N_players roster.
    return (@{
            in_game = $true; is_network = $true; screen_mode = 'game'; current_level_num = 1
            multiplayer = @{
                mission_name = ''; mission_title = 'Descent Demo'; level_num = 1; gamemode_name = 'cooperative'
                network_status = 1; num_connected = 2; num_players = 2; my_player_num = $Slot
                master_player_num = $Master; i_am_master = ($Slot -eq $Master)
                synchronized_object_count = 20; synchronized_unowned_object_count = 20
                synchronized_object_owner_counts = @(0, 0, 0, 0, 0, 0, 0, 0)
                allocated_object_count = 22; active_object_count = 22; last_pdata_received = @(10, 10, 0, 0, 0, 0, 0, 0)
                players = @(@{ slot = 0; callsign = 'lanhost'; is_me = ($Slot -eq 0); connected = 1 },
                    @{ slot = 1; callsign = 'lanjoin'; is_me = ($Slot -eq 1); connected = 1 })
            }
        } | ConvertTo-Json -Depth 20 | ConvertFrom-Json)
}
$PublicShareware = $true
$CALLSIGN1 = 'LanHost'; $CALLSIGN2 = 'LanJoin'
foreach ($master in @(0, 1)) {
    foreach ($slot in @(0, 1)) {
        if (-not (Test-IntroHostRole -Intro (New-MigrationState $slot $master) -LocalSlot $slot -MasterSlot $master -ConnectedPlayers 2 -IsMaster ($slot -eq $master))) {
            throw 'Valid native-D1 serialized peer rejected'
        }
    }
}
foreach ($slot in @(0, 1)) {
    $solo = New-MigrationState $slot $slot
    $solo.multiplayer.num_connected = 1
    $solo.multiplayer.players[1 - $slot].connected = 0
    if (-not (Test-PublicMigrationState -Intro $solo -LocalSlot $slot -MasterSlot $slot -ConnectedPlayers 1 -IsMaster $true -FirstCallsign $CALLSIGN1 -SecondCallsign $CALLSIGN2)) {
        throw 'Valid migrated solo host rejected'
    }
    $solo.multiplayer.players[1 - $slot].connected = 1
    if (Test-PublicMigrationState -Intro $solo -LocalSlot $slot -MasterSlot $slot -ConnectedPlayers 1 -IsMaster $true -FirstCallsign $CALLSIGN1 -SecondCallsign $CALLSIGN2) {
        throw 'Solo host accepted a still-connected remote slot'
    }
}
$lobby = New-MigrationState
$lobby.in_game = $false
$lobby.screen_mode = 'menu'
$lobby.multiplayer.num_players = $lobby.multiplayer.num_connected = 1
$lobby.multiplayer.players = @($lobby.multiplayer.players[0])
if (-not (Test-PublicMigrationLobby -Intro $lobby -Callsign $CALLSIGN1)) { throw 'Natural one-player pre-mine host lobby rejected' }
$lobby.multiplayer.mission_title = 'Descent: First Strike'
if (Test-PublicMigrationLobby -Intro $lobby -Callsign $CALLSIGN1) { throw 'Retail host lobby accepted' }
foreach ($mutate in @(
        { param($s) $s.current_level_num = 2 }, { param($s) $s.screen_mode = 'menu' },
        { param($s) $s.multiplayer.mission_title = 'Descent: First Strike' },
        { param($s) $s.multiplayer.mission_name = 'descent' }, { param($s) $s.multiplayer.level_num = 2 },
        { param($s) $s.multiplayer.gamemode_name = 'anarchy' }, { param($s) $s.multiplayer.num_connected = 1 },
        { param($s) $s.multiplayer.num_players = 3 }, { param($s) $s.multiplayer.players = @($s.multiplayer.players[0]) },
        { param($s) $s.multiplayer.players[1].callsign = 'unknown' },
        { param($s) $s.multiplayer.players[1].connected = 0 }, { param($s) $s.multiplayer.players[1].is_me = $true },
        { param($s) $s.multiplayer.i_am_master = $false }, { param($s) $s.multiplayer.master_player_num = 1 },
        { param($s) $s.multiplayer.synchronized_object_count = 0 },
        { param($s) $s.multiplayer.allocated_object_count = 23 },
        { param($s) $s.PSObject.Properties.Remove('multiplayer') }
    )) {
    $state = New-MigrationState
    & $mutate $state
    if (Test-PublicMigrationState -Intro $state -LocalSlot 0 -MasterSlot 0 -ConnectedPlayers 2 -IsMaster $true -FirstCallsign $CALLSIGN1 -SecondCallsign $CALLSIGN2) {
        throw 'Invalid peer evidence accepted'
    }
}
foreach ($missing in @($null, ('{}' | ConvertFrom-Json), ('{"in_game":true}' | ConvertFrom-Json))) {
    if (Test-PublicMigrationState -Intro $missing -LocalSlot 0 -MasterSlot 0 -ConnectedPlayers 2 -IsMaster $true -FirstCallsign $CALLSIGN1 -SecondCallsign $CALLSIGN2) {
        throw 'Missing peer evidence accepted'
    }
}
function Write-Status { param($Message, $Color) }
function Wait-ForCondition { param($Description, $TimeoutSec, $PollMs, [scriptblock]$Condition) & $Condition }
function Get-GameIntrospection {
    param([string]$Serial, [switch]$Fresh)
    if (-not $Fresh) { throw 'Migration read allowed stale evidence' }
    $slot = if ($Serial -eq 'first') { 0 } else { 1 }
    $state = New-MigrationState $slot
    $script:reads++
    if ($script:case -eq 'missing') { $state.multiplayer.PSObject.Properties.Remove('last_pdata_received') }
    if ($script:case -eq 'stale') { return $null }
    if ($script:reads -gt 2 -and $script:case -ne 'frozen') { $state.multiplayer.last_pdata_received = @(11, 11) }
    if ($script:reads -gt 2 -and $script:case -eq 'one-way' -and $slot -eq 1) { $state.multiplayer.last_pdata_received = @(10, 10) }
    if ($script:reads -gt 2 -and $script:case -eq 'wrong-role') { $state.multiplayer.i_am_master = -not $state.multiplayer.i_am_master }
    return $state
}
foreach ($script:case in @('advancing', 'missing', 'stale', 'frozen', 'one-way', 'wrong-role')) {
    $script:reads = 0
    $passed = Wait-BidirectionalPdata -FirstSerial first -FirstRemoteSlot 1 -SecondSerial second -SecondRemoteSlot 0 -Description 'control'
    if ($passed -ne ($script:case -eq 'advancing')) { throw "PDATA control failed: $script:case" }
}
if (-not (Test-PublicMigrationEngineLog -Log 'Loaded native library: dxx-redux-d1')) { throw 'Native D1 engine log rejected' }
foreach ($log in @('', 'Loaded native library: dxx-redux-d2', "Loaded native library: dxx-redux-d1`nLoaded native library: dxx-redux-d2")) {
    if (Test-PublicMigrationEngineLog -Log $log) { throw 'Missing or wrong engine identity accepted' }
}
$script:commands = @()
$script:badSecond = $false
function Adb-Dev-Timeout {
    param([string]$Serial, [string[]]$AdbArgs, [int]$Seconds)
    $script:commands += "$Serial $($AdbArgs -join ' ')"
    if ($AdbArgs[0] -eq 'get-state') { return 'device' }
    if ($AdbArgs -contains 'getprop') {
        if ($script:badSecond -and $Serial -eq 'emulator-5556') { return '0' }
        return '1'
    }
    throw 'Unexpected device mutation in admission test'
}
Assert-PublicMigrationTargets -HostSerial emulator-5554 -JoinSerial emulator-5556
Assert-Rejected { Assert-PublicMigrationTargets -HostSerial phone -JoinSerial emulator-5556 }
Assert-Rejected { Assert-PublicMigrationTargets -HostSerial emulator-5554 -JoinSerial emulator-5554 }
$script:badSecond = $true
Assert-Rejected { Assert-PublicMigrationTargets -HostSerial emulator-5554 -JoinSerial emulator-5556 }
$script:PublicMigrationTargetsAdmitted = $false
Cleanup
if (@($script:commands | Where-Object { $_ -match 'pm clear|push|force-stop' }).Count) { throw 'Admission mutated device state' }
# Hash mismatch must fail even if adb push/copy return successfully.
$PACKAGE = 'com.dxxredux.app'
$script:DEFAULT_SET_DIR = 'files/imported/sets/default'
$script:hashMismatch = $false
function Adb-Dev-Timeout {
    param([string]$Serial, [string[]]$AdbArgs, [int]$Seconds)
    if ($AdbArgs -contains 'clear') { return 'Success' }
    if ($AdbArgs -contains 'sha256sum') {
        $hash = if ($script:hashMismatch) { 'wrong' } else { 'pinned' }
        return "$hash  $($AdbArgs[-1])"
    }
}
$files = @(@{ File = 'descent.hog'; Source = 'fixture/descent.hog'; Sha256 = 'pinned' },
    @{ File = 'descent.pig'; Source = 'fixture/descent.pig'; Sha256 = 'pinned' })
Stage-PublicMigrationAssets -Serial emulator-5554 -Files $files
$script:hashMismatch = $true
Assert-Rejected { Stage-PublicMigrationAssets -Serial emulator-5554 -Files $files }
Write-Host 'PASS: public native-D1 migration profiles, serialized exact peers, fresh bidirectional PDATA, and disposable admission'
