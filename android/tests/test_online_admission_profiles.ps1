#!/usr/bin/env pwsh
# Device-free controls for the registered online owner's explicit consent boundary
[Diagnostics.CodeAnalysis.SuppressMessageAttribute('PSAvoidOverwritingBuiltInCmdlets', '', Justification = 'Owned test stubs simulate readiness and elapsed time without devices or network')]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$errors = $null
$owner = [Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'test_mp.ps1'), [ref]$null, [ref]$errors)
if ($errors.Count) { throw 'Online owner parse failed' }
foreach ($name in @('Test-ExpectedOnlineAdmission', 'Assert-OnlineAdmissionCancelled')) {
    $definition = $owner.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $true)
    if (-not $definition) { throw "Missing online function $name" }
    . ([scriptblock]::Create($definition.Extent.Text))
}
foreach ($case in @('valid', 'wrong-call', 'wrong-id', 'wrong-slot', 'expired', 'negative-age', 'nonhost', 'not-pending', 'missing-field')) {
    $game = [pscustomobject]@{ in_game = $true; is_network = $true; multiplayer = [pscustomobject]@{
            i_am_master = $true; join_request_pending = $true; join_request_callsign = 'LatePlt'; join_request_age_ms = 100
        }
    }
    $control = [pscustomobject]@{ lobby = [pscustomobject]@{ players = @(
                [pscustomobject]@{ player_id = 'fixture-player'; callsign = 'LatePlt'; slot = 2 }
            )
        }
    }
    switch ($case) {
        'wrong-call' { $game.multiplayer.join_request_callsign = 'Unrelated' }
        'wrong-id' { $control.lobby.players[0].player_id = 'another-player' }
        'wrong-slot' { $control.lobby.players[0].slot = 1 }
        'expired' { $game.multiplayer.join_request_age_ms = 6000 }
        'negative-age' { $game.multiplayer.join_request_age_ms = -1 }
        'nonhost' { $game.multiplayer.i_am_master = $false }
        'not-pending' { $game.multiplayer.join_request_pending = $false }
        'missing-field' { $game.multiplayer.PSObject.Properties.Remove('join_request_callsign') }
    }
    $accepted = Test-ExpectedOnlineAdmission -GameState $game -Control $control -PlayerId 'fixture-player' -Callsign 'LatePlt' -Slot 2
    if ($accepted -ne ($case -eq 'valid')) { throw "Consent guard result: $case" }
}
$fixture = Get-Content (Join-Path $PSScriptRoot '../game_scripts/test_online_late_join_accept.jsonc') -Raw | ConvertFrom-Json
if ($fixture.Count -ne 2 -or $fixture[0]._info._owner -cne 'test_mp' -or $fixture[0]._info._standalone -ne $false -or
    $fixture[1].action -cne 'key' -or $fixture[1].key -cne 'f6') { throw 'Online consent must use the real F6 producer' }
. "$PSScriptRoot/../helpers/test_suite_coverage.ps1"
if ('test_mp' -notin (Get-TestSuiteInstrumentationOwners)) { throw 'Online owner must provision device work instrumentation' }

$PublicDirect = $false; $script:onlineHostTimeoutCount = 0
$EMU1 = 'emulator-5554'; $EMU2 = 'emulator-5556'
$CALLSIGN1 = 'HostPlt'; $CALLSIGN2 = 'JoinPlt'; $PACKAGE = 'com.dxxredux.app'
function Send-MpCommand { param($Serial, $Command) if ($Command -ne 'disconnect') { throw 'Cancellation sent an approval' } }
function Wait-ForCondition { param($Description, $TimeoutSec, $PollMs, $Condition) return (& $Condition) }
function Invoke-RestMethod {
    param($Uri, $TimeoutSec)
    return [pscustomobject]@{ rooms = 1; relay_sessions = $(if ($script:leak) { 3 } else { 1 }); pending_and_authenticated_sockets = 2 }
}
function Get-MpIntrospection {
    param($Serial)
    return [pscustomobject]@{ online_resources = [pscustomobject]@{
            proxy_workers = 0; background_jobs = 0; candidate_socket = $false; upnp_mapping = $false; peers = @()
        }
    }
}
function Get-GameIntrospection { param($Serial) return [pscustomobject]@{ multiplayer = [pscustomobject]@{ join_request_pending = $false } } }
function Start-Sleep { param($Seconds) if ($Seconds -ne 12) { throw 'Survival dwell changed' } }
function Assert-OnlinePeerMatrix {
    param($Serials, $Callsigns, $Slots, $Phase)
    if (($Slots -join ',') -cne '0,1' -or $Serials.Count -ne 2) { throw 'Cancellation changed survivor matrix' }
    $script:survivors++
}
function Adb-Dev-Timeout { param($Serial, $AdbArgs, $Seconds) $script:stops++ }
function Write-Status { param($Message, $Color) }
foreach ($leak in @($false, $true)) {
    $script:leak = $leak; $script:survivors = 0; $script:stops = 0; $failed = $false
    try { Assert-OnlineAdmissionCancelled -Serial 'emulator-5558' } catch { $failed = $true }
    if ($failed -ne $leak) { throw 'Cancellation failed to enforce remaining server resource counts' }
    if ($leak -and ($script:stops -ne 0 -or $script:survivors -ne 0)) { throw 'Cleanup masked an orphan relay pair' }
    if (-not $leak -and ($script:stops -ne 1 -or $script:survivors -ne 1)) { throw 'Cancellation skipped surviving gameplay oracle' }
}
# No cancel command is permitted in the native terminal path, even on failure
function Send-MpCommand { param($Serial, $Command) throw 'Native timeout sent a client cancel' }
function Invoke-RestMethod {
    param($Uri, $TimeoutSec)
    return [pscustomobject]@{ rooms = 1; relay_sessions = 1; pending_and_authenticated_sockets = 3 }
}
function Adb-Dev-Timeout {
    param($Serial, $AdbArgs, $Seconds)
    if ($AdbArgs[0] -eq 'logcat') {
        if ($script:nativeReceipt) { return $(if ($Serial -eq $EMU1) { 'ONLINE_ADMISSION event=host_consent_timeout' } else { 'ONLINE_ADMISSION event=host_refusal' }) }
        return 'unrelated log line'
    }
    if (($AdbArgs -join ' ') -notmatch 'force-stop') { throw 'Unexpected terminal control' }
    $script:stops++
}
foreach ($receipt in @($true, $false)) {
    $script:nativeReceipt = $receipt; $script:survivors = 0; $script:stops = 0; $failed = $false
    try { Assert-OnlineAdmissionCancelled -Serial 'emulator-5558' -NativeTimeout } catch { $failed = $true }
    if ($failed -eq $receipt) { throw 'Native timeout did not require a real producer receipt' }
    if ($receipt -and ($script:stops -ne 1 -or $script:survivors -ne 1)) { throw 'Native timeout skipped survivor proof' }
    if (-not $receipt -and ($script:stops -ne 0 -or $script:survivors -ne 0)) { throw 'Cleanup masked a missing native producer' }
}
Write-Host 'PASS: explicit online fixture consent, stale/unrelated rejection, cancellation isolation and instrumentation ownership'
# The failure observer must inspect every client, even when the first roster is wrong
foreach ($name in @('Get-OnlinePeerSample', 'Assert-OnlinePeerMatrix')) {
    $definition = $owner.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $true)
    . ([scriptblock]::Create($definition.Extent.Text))
}
$allTargets = @($EMU1, $EMU2, 'emulator-5558')
$labels = @('HostPlt', 'JoinPlt', 'LatePlt')
function Get-GameIntrospection {
    param($Serial)
    $script:tick++
    $roster = @($labels)
    if ($script:wrongRoster -and $Serial -eq $EMU1) { $roster = @($labels[0], $labels[1]) }
    return [pscustomobject]@{
        in_game = $true; is_network = $true; multiplayer = [pscustomobject]@{
            join_request_pending = $false; players = @($roster | ForEach-Object { [pscustomobject]@{ connected = 1; callsign = $_ } })
        }
    }
}
function Get-MpIntrospection {
    param($Serial)
    $slot = [array]::IndexOf($allTargets, $Serial)
    $peers = @(0..2 | Where-Object { $_ -ne $slot } | ForEach-Object {
            [pscustomobject]@{ slot = $_; bound = $true; sent = $script:tick * 10; gameplay_received = $script:tick * 10 }
        })
    return [pscustomobject]@{
        player_id = 'self'; lobby = [pscustomobject]@{ players = @([pscustomobject]@{ player_id = 'self'; slot = $slot }) }
        online_resources = [pscustomobject]@{ peers = $peers }
    }
}
function Wait-ForCondition {
    param($Description, $TimeoutSec, $PollMs, $Condition)
    $null = & $Condition
    return (& $Condition)
}
function Write-Status { param($Message, $Color) $script:snapshots += $Message }
foreach ($wrong in @($false, $true)) {
    $script:wrongRoster = $wrong; $script:tick = 0; $script:snapshots = @(); $failed = $false
    try { Assert-OnlinePeerMatrix -Serials $allTargets -Callsigns $labels -Slots @(0, 1, 2) -Phase 'fixture' } catch { $failed = $true }
    if ($failed -ne $wrong) { throw 'Peer oracle acceptance changed' }
    if (@($script:snapshots | Where-Object { $_ -like 'ONLINE_STATE *' }).Count -ne 6) { throw 'First-client failure hid other clients' }
}
Write-Host 'PASS: every client is observed while native roster and gameplay checks remain strict'

# Model Android Instrument.TextStatusReporter: stream-only pretty output omits the terminal code
$definition = $owner.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Measure-InstallationWork'
    }, $true)
. ([scriptblock]::Create($definition.Extent.Text))
function Adb-Dev-Timeout {
    param($Serial, $AdbArgs, $Seconds)
    if ($AdbArgs -notcontains 'instrument') { return '' }
    $script:vectors++
    $stream = "ONLINE_WORK_VECTOR difficulty=20 hashes=4243438 elapsed_ms=7000 solved=true`n"
    if ($script:vectorCase -ne 'missing-pass') { $stream += "PASS: installation work fixture and cancellation`n" }
    if ($AdbArgs -contains '-r' -and $script:vectorCase -ne 'missing-code') {
        $code = if ($script:vectorCase -eq 'failed-code') { 0 } else { -1 }
        return "INSTRUMENTATION_RESULT: stream=$stream`nINSTRUMENTATION_CODE: $code"
    }
    return $stream
}
function Start-SetupActivity { param($Serial) return $true }
function Send-MpCommand { param($Serial, $Command, $Extras) if ($Command -eq 'connect') { $script:attempts++ } }
function Get-MpIntrospection {
    param($Serial)
    return [pscustomobject]@{ status = 'CONNECTED'; error = $null; online_resources = [pscustomobject]@{
            installation_work = [pscustomobject]@{ attempt = 1; difficulty = 20; hashes = 100; elapsed_ms = 1; solved = $true }
        }
    }
}
function Wait-ForCondition { param($Description, $TimeoutSec, $PollMs, $Condition) return (& $Condition) }
function Invoke-RestMethod { param($Uri, $TimeoutSec) return [pscustomobject]@{ pending_and_authenticated_sockets = 0; rooms = 0; relay_sessions = 0 } }
foreach ($case in @('raw-pass', 'missing-code', 'failed-code', 'missing-pass')) {
    $script:vectorCase = $case; $script:vectors = 0; $script:attempts = 0; $script:snapshots = @(); $failed = $false
    try { Measure-InstallationWork -ServerUrl 'ws://127.0.0.1:9000/ws' } catch { $failed = $true }
    if ($failed -ne ($case -ne 'raw-pass')) { throw "Instrumentation terminal evidence result: $case" }
    if ($script:vectors -ne 3 -or $script:attempts -ne 12) { throw 'Work campaign skipped required samples' }
    if (@($script:snapshots | Where-Object { $_ -like 'ONLINE_WORK_RESULT *' }).Count -ne 3) { throw 'Missing bounded vector result diagnostics' }
}
Write-Host 'PASS: actual raw instrumentation output is required; solved work alone cannot qualify the fixture'

$returnDefinition = $owner.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Invoke-OnlineReturnToGame'
    }, $true)
if (-not $returnDefinition) { throw 'Missing Return to Game owner' }
. ([scriptblock]::Create($returnDefinition.Extent.Text))
$ACTIVITY = 'com.dxxredux.app.SetupActivity'
function Start-SetupActivity { throw 'Return to Game must not use the force-stop helper' }
function Wait-SetupReady { param($Serial, $TimeoutSec) return $true }
function Adb-Dev-Timeout {
    param($Serial, $AdbArgs, $Seconds)
    if ($AdbArgs -contains 'force-stop') { throw 'Return to Game destroyed the owned process' }
    if ($AdbArgs -contains 'start') { $script:returnStarted = $true }
    if ($AdbArgs -contains 'pidof') {
        if ($script:returnStarted -and $script:returnCase -eq 'replaced-pid') { return '999' }
        return '123'
    }
    return ''
}
function Get-GameIntrospection {
    param($Serial, [switch]$Fresh)
    if (-not $Fresh) { throw 'Return to Game accepted cached native evidence' }
    return [pscustomobject]@{ in_game = $true; is_network = $true; multiplayer = [pscustomobject]@{ my_player_num = 0 } }
}
function Get-MpIntrospection {
    param($Serial)
    $room = if ($script:returnStarted -and $script:returnCase -eq 'replaced-room') { 'new-room' } else { 'original-room' }
    $epoch = if ($script:returnStarted -and $script:returnCase -eq 'replaced-attempt') { 2 } else { 1 }
    return [pscustomobject]@{ lobby = [pscustomobject]@{ lobby_id = $room }; online_resources = [pscustomobject]@{ runtime_attempt_epoch = $epoch } }
}
function Assert-OnlinePeerMatrix { param($Serials, $Callsigns, $Slots, $Phase) $script:returnTraffic = $true }
foreach ($case in @('preserved', 'replaced-pid', 'replaced-room', 'replaced-attempt')) {
    $script:returnCase = $case; $script:returnStarted = $false; $script:returnTraffic = $false; $failed = $false
    try { Invoke-OnlineReturnToGame } catch { $failed = $true }
    if ($failed -ne ($case -ne 'preserved')) { throw "Return to Game ownership control failed: $case" }
    if ($case -eq 'preserved' -and -not $script:returnTraffic) { throw 'Return to Game omitted survivor traffic' }
}
. ([scriptblock]::Create($returnDefinition.Extent.Text.Replace(' -Fresh', '')))
$script:returnCase = 'preserved'; $script:returnStarted = $false; $failed = $false
try { Invoke-OnlineReturnToGame } catch { $failed = $true }
if (-not $failed) { throw 'Cached-native-evidence negative control passed unexpectedly' }
Write-Host 'PASS: Return to Game preserves owner lifetimes and requires fresh native and survivor evidence'


$outageDefinition = $owner.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Test-OnlineOutageTerminal'
    }, $true)
if (-not $outageDefinition) { throw 'Missing bounded outage predicate' }
. ([scriptblock]::Create($outageDefinition.Extent.Text))
function Get-MpIntrospection {
    param($Serial)
    if ($script:outageCase -eq 'missing-control') { return $null }
    $snapshot = [pscustomobject]@{ error = 'Network forwarding stopped'; online_resources = [pscustomobject]@{ proxy_workers = 0 } }
    if ($script:outageCase -eq 'missing-error') { $snapshot.error = $null }
    if ($script:outageCase -eq 'room-retained') { $snapshot | Add-Member lobby ([pscustomobject]@{}) }
    if ($script:outageCase -eq 'workers-retained') { $snapshot.online_resources.proxy_workers = 1 }
    if ($script:outageCase -eq 'missing-workers') { $snapshot.online_resources.PSObject.Properties.Remove('proxy_workers') }
    return $snapshot
}
function Adb-Dev-Timeout {
    param($Serial, $AdbArgs, $Seconds)
    if (($AdbArgs -join ' ') -cne 'get-state' -or $Seconds -ne 2) { throw 'Outage probe mutated device or lost its bound' }
    return 'offline'
}
function Write-Status { param($Message, $Color) $script:outageReceipt = $Message }
foreach ($case in @('terminal', 'missing-error', 'room-retained', 'workers-retained', 'missing-workers', 'missing-control')) {
    $script:outageCase = $case; $script:outageReceipt = ''
    $accepted = Test-OnlineOutageTerminal -Serial 'emulator-5554'
    if ($accepted -ne ($case -eq 'terminal')) { throw "Outage terminal predicate failed: $case" }
    if ($script:outageReceipt -notmatch '^ONLINE_OUTAGE client=emulator-5554 adb=') { throw 'Missing outage timing receipt' }
    if ($case -eq 'missing-control' -and $script:outageReceipt -notmatch 'adb=offline control=False') { throw 'Device loss was not distinguished from control state' }
}
Write-Host 'PASS: bounded outage retains strict visible-error, room and worker cleanup; device absence never qualifies'

foreach ($name in @('Test-OnlineOutageHostSurvival', 'Assert-OnlineUdpOutage')) {
    $definition = $owner.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $true)
    if (-not $definition) { throw "Missing outage owner function $name" }
    . ([scriptblock]::Create($definition.Extent.Text))
    if ($name -eq 'Assert-OnlineUdpOutage') { $outageOwnerDefinition = $definition }
}
function Get-MpIntrospection {
    param($Serial)
    if ($script:outageDropped -and $Serial -eq $EMU1 -and $script:outageCase -eq 'missing-host') { return $null }
    if ($script:outageDropped -and ($Serial -eq $EMU2 -or $script:outageCase -eq 'both-terminal')) {
        $failureMessage = if ($script:outageCase -eq 'joiner-no-error') { $null } else { 'Visible network failure' }
        return [pscustomobject]@{ error = $failureMessage; online_resources = [pscustomobject]@{ proxy_workers = 0 } }
    }
    $result = [pscustomobject]@{ player_id = 'host-id'; lobby = [pscustomobject]@{
            lobby_id = 'owned-room'; is_host = $true; players = @([pscustomobject]@{ player_id = 'host-id'; slot = 0 })
        }; online_resources = [pscustomobject]@{ runtime_attempt_epoch = 1; peers = @(); proxy_workers = 1 }
    }
    if ($script:outageDropped) {
        switch ($script:outageCase) {
            'replacement-attempt' { $result.online_resources.runtime_attempt_epoch = 2 }
            'replacement-room' { $result.lobby.lobby_id = 'other-room' }
            'retained-peer' { $result.online_resources.peers = @([pscustomobject]@{ slot = 1 }) }
            'retained-workers' { $result.online_resources.proxy_workers = 2 }
            'nonhost-control' { $result.lobby.is_host = $false }
            'extra-member' { $result.lobby.players += [pscustomobject]@{ player_id = 'old-peer'; slot = 1 } }
        }
    }
    return $result
}
function Get-GameIntrospection {
    param($Serial, [switch]$Fresh)
    if (-not $Fresh) { throw 'Outage host survival used cached native state' }
    $result = [pscustomobject]@{ in_game = $true; is_network = $true; game_window_is_front = $true; multiplayer = [pscustomobject]@{
            i_am_master = $true; my_player_num = 0; join_request_pending = $false
            players = @([pscustomobject]@{ callsign = 'HostPlt'; connected = 1 })
        }
    }
    switch ($script:outageCase) {
        'native-menu' { $result.in_game = $false }
        'native-modal' { $result.game_window_is_front = $false }
        'native-nonhost' { $result.multiplayer.i_am_master = $false }
        'native-extra-peer' { $result.multiplayer.players += [pscustomobject]@{ callsign = 'JoinPlt'; connected = 1 } }
    }
    return $result
}
function Invoke-RestMethod {
    param($Uri, $TimeoutSec)
    return [pscustomobject]@{
        rooms = $(if ($script:outageCase -eq 'both-terminal') { 0 } else { 1 })
        relay_sessions = $(if ($script:outageCase -eq 'server-relay-leak') { 1 } else { 0 })
        pending_native_admissions = 0; pending_and_authenticated_sockets = 2
    }
}
function Set-OnlineFault { param($Udp) if ($Udp -cne 'drop') { throw 'Unexpected outage action' }; $script:outageDropped = $true }
function Wait-ForCondition {
    param($Description, $TimeoutSec, $PollMs, $Condition)
    if ($TimeoutSec -ne 25 -or $PollMs -ne 500) { throw 'Outage bound changed' }
    return (& $Condition)
}
function Write-OnlineOutageDiagnostics { $script:outageDiagnostic = $true }
foreach ($case in @('both-terminal', 'live-host-only', 'missing-host', 'joiner-no-error', 'replacement-attempt',
        'replacement-room', 'retained-peer', 'retained-workers', 'nonhost-control', 'extra-member',
        'native-menu', 'native-modal', 'native-nonhost', 'native-extra-peer', 'server-relay-leak')) {
    $script:outageCase = $case; $script:outageDropped = $false; $script:outageDiagnostic = $false; $failed = $false
    try { Assert-OnlineUdpOutage } catch { $failed = $true }
    $expectedFailure = $case -notin @('both-terminal', 'live-host-only')
    if ($failed -ne $expectedFailure) { throw "Outage ownership/cleanup control failed: $case" }
    if ($expectedFailure -and -not $script:outageDiagnostic) { throw "Outage failure lost pre-cleanup evidence: $case" }
}
. ([scriptblock]::Create($outageOwnerDefinition.Extent.Text.Replace(' -Fresh', '')))
$script:outageCase = 'live-host-only'; $script:outageDropped = $false; $failed = $false
try { Assert-OnlineUdpOutage } catch { $failed = $true }
if (-not $failed) { throw 'Outage accepted cached host evidence' }
Write-Host 'PASS: real outage owner keeps 25-second bound, requires affected-peer error/cleanup and fresh exact-owner native host survival'
