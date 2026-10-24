# Opt-in online session adapters; the shared native scenario is transport-independent
$script:onlineProbeFaults = @{}

function Invoke-TransportAdb {
    param([string]$Serial, [string[]]$Arguments)
    $previous = $env:ANDROID_SERIAL
    try {
        $env:ANDROID_SERIAL = $Serial
        $diagnostic = $null
        $output = Adb-Timeout -AdbArgs $Arguments -Seconds 15 -DiagnosticResult ([ref]$diagnostic)
        if (-not $diagnostic -or $diagnostic.timed_out -or $diagnostic.exit_code -ne 0) {
            throw "Owned transport fixture command failed on $Serial"
        }
        return $output
    } finally { $env:ANDROID_SERIAL = $previous }
}

function Install-OnlineProbeFault {
    param([string]$Serial)
    if (-not $PublicShareware -or $Serial -notmatch '^emulator-[0-9]+$') { throw 'Probe faults require disposable emulators' }
    $identity = Invoke-TransportAdb -Serial $Serial -Arguments @('shell', 'cmd', 'package', 'list', 'packages', '-U', $PACKAGE)
    if ($identity -notmatch ('(?m)^package:' + [regex]::Escape($PACKAGE) + ' uid:([0-9]+)\s*$')) { throw 'Cannot resolve fixture application UID' }
    $uid = $Matches[1]
    Invoke-TransportAdb -Serial $Serial -Arguments @('push', (Join-Path $REPO_ROOT 'android/helpers/online_probe_fault.sh'), '/data/local/tmp/dxx-online-probe-fault.sh') | Out-Null
    # Remember the UID before installation so a lost ADB response cannot hide an installed rule
    $script:onlineProbeFaults[$Serial] = @($uid, ('DXX_T_' + [guid]::NewGuid().ToString('N').Substring(0, 8)))
    Invoke-TransportAdb -Serial $Serial -Arguments @('shell', 'su', '0', 'sh', '/data/local/tmp/dxx-online-probe-fault.sh', 'install', $uid, $script:onlineProbeFaults[$Serial][1]) | Out-Null
}

function Remove-OnlineProbeFaults {
    $errors = @()
    foreach ($serial in @($script:onlineProbeFaults.Keys)) {
        try {
            Invoke-TransportAdb -Serial $serial -Arguments @('shell', 'su', '0', 'sh', '/data/local/tmp/dxx-online-probe-fault.sh', 'remove', $script:onlineProbeFaults[$serial][0], $script:onlineProbeFaults[$serial][1]) | Out-Null
            $script:onlineProbeFaults.Remove($serial)
        } catch { $errors += $serial }
    }
    if ($errors.Count) { throw "Probe fixture cleanup failed on $($errors -join ',')" }
}

function Test-OnlineFallbackProbe {
    param($Control, [string]$RoomId, [string]$PlayerId, [string]$PeerId)
    if (-not $Control) { return $false }
    try {
        $probe = $Control.online_resources.connectivity
        $sent = @($probe.probe_counts.PSObject.Properties | Where-Object { $_.Name -match '^sent_[0-9]+$' -and $_.Value -gt 0 })
        $matched = $probe.probe_counts.PSObject.Properties['matched']
        if ($sent.Count -eq 0 -or ($matched -and $matched.Value -ne 0)) { return $false }
        return Test-OnlineDirectProbe -Control $Control -RoomId $RoomId -PlayerId $PlayerId -PeerId $PeerId -CandidateTypes @('relay')
    } catch { return $false }
}

function Wait-OnlineInitialFallback {
    $hostControl = Get-MpIntrospection -Serial $EMU1
    $joinControl = Get-MpIntrospection -Serial $EMU2
    $room = [string]$hostControl.lobby.lobby_id
    $ids = @([string]$hostControl.player_id, [string]$joinControl.player_id)
    if (-not (Wait-ForCondition -Description 'Both current direct probes fail into relay' -TimeoutSec 30 -PollMs 500 -Condition {
                $a = Test-OnlineFallbackProbe -Control (Get-MpIntrospection -Serial $EMU1) -RoomId $room -PlayerId $ids[0] -PeerId $ids[1]
                $b = Test-OnlineFallbackProbe -Control (Get-MpIntrospection -Serial $EMU2) -RoomId $room -PlayerId $ids[1] -PeerId $ids[0]
                return $a -and $b
            })) { throw 'Initial fallback did not prove failed current direct probes on both peers' }
    foreach ($serial in @($EMU1, $EMU2)) {
        $count = Invoke-TransportAdb -Serial $serial -Arguments @('shell', 'su', '0', 'sh', '/data/local/tmp/dxx-online-probe-fault.sh', 'count', $script:onlineProbeFaults[$serial][0], $script:onlineProbeFaults[$serial][1])
        if ($count -notmatch '^[0-9]+$' -or [long]$count -le 0) { throw 'Initial fallback fixture did not drop direct UDP traffic' }
        Write-Status "ONLINE_INITIAL_FALLBACK dropped=$count current_probe_failed=True forced_relay=False" 'Gray'
    }
}

function Test-OnlineTransportRoute {
    param($Resources, [string]$Kind, [int]$Slot, [long]$ExpectedEpoch = 0)
    try {
        $routes = @($Resources.peers)
        return $routes.Count -eq 1 -and $routes[0].slot -eq $Slot -and
        $routes[0].route_kind -ceq $Kind -and $routes[0].route_epoch -gt 0 -and
        $routes[0].route_epoch -eq $Resources.runtime_attempt_epoch -and
        ($ExpectedEpoch -eq 0 -or $routes[0].route_epoch -eq $ExpectedEpoch) -and
        $routes[0].gameplay_received -gt 0
    } catch { return $false }
}

function Assert-OnlineTransportGraph {
    param([hashtable]$ExpectedEpochs = @{})
    $kind = if ($TransportMode -eq 'ServerDirect') { 'direct' } else { 'relay' }
    $epochs = @{}
    foreach ($serial in @($EMU1, $EMU2)) {
        $control = Get-MpIntrospection -Serial $serial
        $slot = if ($serial -eq $EMU1) { 1 } else { 0 }
        $expected = if ($ExpectedEpochs.ContainsKey($serial)) { [long]$ExpectedEpochs[$serial] } else { 0 }
        if (-not (Test-OnlineTransportRoute -Resources $control.online_resources -Kind $kind -Slot $slot -ExpectedEpoch $expected)) {
            throw 'Opt-in transport graph differs from the current native pair'
        }
        $epochs[$serial] = [long]$control.online_resources.runtime_attempt_epoch
        Write-Status "ONLINE_TRANSPORT_ROUTE peer_slot=$slot kind=$kind epoch=$($epochs[$serial])" 'Gray'
    }
    $status = Invoke-RestMethod -Uri 'http://127.0.0.1:8080/api/v1/ready' -TimeoutSec 2
    $leases = if ($kind -eq 'direct') { 0 } else { 1 }
    if ($status.relay_sessions -ne $leases) { throw 'Opt-in transport graph has unexpected relay leases' }
    return $epochs
}
