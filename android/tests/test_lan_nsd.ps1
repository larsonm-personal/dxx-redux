# NSD-only discovery, lifecycle restart, deduplication, and real lobby joins
# Does not clear app data, saved addresses, missions, or saved games
# Both devices require the diagnostic debuggable APK and imported D1/D2 data
param(
    [Parameter(Mandatory)][string]$HostSerial,
    [Parameter(Mandatory)][string]$ClientSerial,
    [ValidateSet('d1', 'd2')][string]$Game = 'd2',
    [int]$TimeoutSeconds = 60,
    [switch]$ReconnectCoverage,
    [ValidateSet('com.dxxredux.app', 'com.dxxredux.app.nsdtest')][string]$AppPackage = 'com.dxxredux.app',
    [switch]$ProvisionDiagnosticData
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$script:PACKAGE = $AppPackage
if ($ProvisionDiagnosticData -and $AppPackage -ne 'com.dxxredux.app.nsdtest') {
    throw 'Automatic provisioning is restricted to the separate diagnostic app'
}
if ($HostSerial -eq $ClientSerial) { throw 'Two distinct devices are required' }
$outDir = Join-Path (Split-Path $PSScriptRoot) 'temp/nsd-test'
New-Item -ItemType Directory -Force $outDir | Out-Null
$script:LogFile = Join-Path $outDir 'runner.log'
Set-Content -LiteralPath $script:LogFile -Value ''
$serials = @($HostSerial, $ClientSerial)
$script:NsdTimeoutSeconds = $TimeoutSeconds
$results = [System.Collections.Generic.List[object]]::new()
$stayOn = @{}

function Read-NsdLog([string]$Serial) {
    return Adb-Dev-Timeout -Serial $Serial -Seconds 10 -AdbArgs @(
        'logcat', '-d', '-v', 'threadtime', '-s', 'DXX-MP:*', 'LobbyService:*', 'LanNsdDiscovery:*')
}

function Wait-Nsd([string]$Description, [scriptblock]$Condition) {
    if (-not (Wait-ForCondition -Description $Description -TimeoutSec $script:NsdTimeoutSeconds -PollMs 1000 -Condition $Condition)) {
        throw "Timed out: $Description"
    }
}

function Invoke-NsdRoundCleanup {
    foreach ($serial in $serials) { Send-MpCommand -Serial $serial -Command 'lan_stop_lobby' }
}

try {
    foreach ($serial in $serials) {
        if (-not (Test-DeviceOnline -Serial $serial)) { throw "Device not connected: $serial" }
        $packageInfo = Adb-Dev-Timeout -Serial $serial -Seconds 10 -AdbArgs @('shell', 'dumpsys', 'package', $script:PACKAGE)
        if ($packageInfo -notmatch 'DEBUGGABLE') { throw "$serial needs the diagnostic debug APK; do not uninstall or clear app data" }
        $stayOn[$serial] = (Adb-Dev-Timeout -Serial $serial -Seconds 5 -AdbArgs @('shell', 'settings', 'get', 'global', 'stay_on_while_plugged_in')).Trim()
        Adb-Dev-Timeout -Serial $serial -Seconds 5 -AdbArgs @('shell', 'svc', 'power', 'stayon', 'usb') | Out-Null
        if ($AppPackage -eq 'com.dxxredux.app.nsdtest') {
            # The normal installation must release UDP 42400 while the diagnostic app runs
            Adb-Dev-Timeout -Serial $serial -Seconds 5 -AdbArgs @('shell', 'am', 'force-stop', 'com.dxxredux.app') | Out-Null
        }
        if ($ProvisionDiagnosticData -and -not (Ensure-StandardGameDataOnDevice -Serial $serial)) {
            throw "Could not provision diagnostic game files on $serial"
        }
        if (-not (Start-SetupActivity -Serial $serial)) { throw "Launcher not ready on $serial; unlock the phone" }
    }
    $rounds = @(
        @{ Name = 'nsd_forward'; HostDevice = $HostSerial; ClientDevice = $ClientSerial; Only = $true; Legacy = $false },
        @{ Name = 'nsd_reverse'; HostDevice = $ClientSerial; ClientDevice = $HostSerial; Only = $true; Legacy = $false },
        @{ Name = 'nsd_legacy'; HostDevice = $HostSerial; ClientDevice = $ClientSerial; Only = $true; Legacy = $true },
        @{ Name = 'combined'; HostDevice = $HostSerial; ClientDevice = $ClientSerial; Only = $false; Legacy = $false }
    )
    foreach ($round in $rounds) {
        Write-Status "Starting $($round.Name)" 'Cyan'
        Invoke-NsdRoundCleanup
        foreach ($serial in $serials) {
            Adb-Dev-Timeout -Serial $serial -Seconds 5 -AdbArgs @('logcat', '-c') | Out-Null
            Send-MpCommand -Serial $serial -Command 'lan_nsd_only' -Extras @(
                '--ez', 'enabled', "$($round.Only)".ToLowerInvariant(), '--ez', 'legacy', "$($round.Legacy)".ToLowerInvariant())
        }
        $hostDevice = $round.HostDevice
        $clientDevice = $round.ClientDevice
        foreach ($serial in $serials) {
            $expectedMode = "NSD-only test mode=$($round.Only) legacy=$($round.Legacy)".ToLowerInvariant()
            if ((Read-NsdLog $serial).ToLowerInvariant() -notmatch [regex]::Escape($expectedMode)) {
                throw "Discovery test mode was not acknowledged on $serial"
            }
        }
        $hostName = 'NsdHost'
        Send-MpCommand -Serial $hostDevice -Command 'lan_host_lobby' -Extras @(
            '--es', 'callsign', $hostName, '--es', 'game', $Game,
            '--es', 'mission', $(if ($Game -eq 'd1') { 'descent' } else { 'd2' }), '--es', 'mode', 'coop')
        Send-MpCommand -Serial $clientDevice -Command 'lan_discover' -Extras @('--es', 'callsign', 'NsdClient')
        Wait-Nsd 'NSD registration' { (Read-NsdLog $hostDevice) -match 'Registered lobby=' }
        Wait-Nsd 'NSD resolved and confirmed by UDP' {
            $log = Read-NsdLog $clientDevice
            return $log -match 'Resolved lobby=' -and $log -match 'Discovery confirmed source=nsd'
        }
        if ($round.Legacy -and (Read-NsdLog $clientDevice) -notmatch 'Resolving legacy service=') {
            throw 'Legacy resolution path was not exercised'
        }
        if ($round.Name -eq 'nsd_forward') {
            if ($ReconnectCoverage) {
                Adb-Dev-Timeout -Serial $clientDevice -Seconds 5 -AdbArgs @('logcat', '-c') | Out-Null
                try {
                    Adb-Dev-Timeout -Serial $clientDevice -Seconds 5 -AdbArgs @('shell', 'svc', 'wifi', 'disable') | Out-Null
                    Start-Sleep -Seconds 3
                } finally {
                    Adb-Dev-Timeout -Serial $clientDevice -Seconds 5 -AdbArgs @('shell', 'svc', 'wifi', 'enable') | Out-Null
                }
                Wait-Nsd 'fresh NSD confirmation after Wi-Fi reconnect' {
                    $log = Read-NsdLog $clientDevice
                    return $log -match 'Resolved lobby=' -and $log -match 'Discovery confirmed source=nsd'
                }
                Write-Status 'PASS: Wi-Fi reconnect recovered NSD discovery' 'Green'
            }
            Adb-Dev-Timeout -Serial $clientDevice -Seconds 5 -AdbArgs @('logcat', '-c') | Out-Null
            Send-MpCommand -Serial $hostDevice -Command 'lan_nsd_suspend' -Extras @('--ez', 'enabled', 'true')
            Wait-Nsd 'client observes withdrawn mDNS advertisement' { (Read-NsdLog $clientDevice) -match 'Lost service=' }
            Adb-Dev-Timeout -Serial $hostDevice -Seconds 5 -AdbArgs @('logcat', '-c') | Out-Null
            # A repeated discovery request must not undo an explicit test suspension
            Send-MpCommand -Serial $hostDevice -Command 'lan_discover' -Extras @('--es', 'callsign', $hostName)
            # Exceed the host's 30-second query-peer retention; direct probes must maintain the lobby
            Start-Sleep -Seconds 35
            if ((Read-NsdLog $hostDevice) -match 'Registered lobby=') { throw 'Discovery refresh undid NSD suspension' }
            Send-MpCommand -Serial $clientDevice -Command 'lan_discover_status'
            $withdrawnStatus = ((Read-NsdLog $clientDevice) -split "`r?`n" | Where-Object { $_ -match 'lan_discover_status:' } | Select-Object -Last 1)
            if ($withdrawnStatus -notmatch 'lobbies=1 hosting=false') { throw 'A reachable lobby expired when only mDNS was lost' }
            Send-MpCommand -Serial $hostDevice -Command 'lan_nsd_suspend' -Extras @('--ez', 'enabled', 'false')
            Wait-Nsd 'mDNS advertisement restored' { (Read-NsdLog $clientDevice) -match 'Resolved lobby=' }
            Write-Status 'PASS: unicast liveness survives mDNS loss and advertisement restart' 'Green'
        }
        # Let periodic broadcasts, cached NSD callbacks, and direct replies all arrive
        Start-Sleep -Seconds 12
        Send-MpCommand -Serial $clientDevice -Command 'lan_discover_status'
        $log = Read-NsdLog $clientDevice
        $status = ($log -split "`r?`n" | Where-Object { $_ -match 'lan_discover_status:' } | Select-Object -Last 1)
        if ($status -notmatch 'lobbies=1 hosting=false') { throw "Expected one deduplicated lobby: $status" }
        if ($round.Only) {
            foreach ($serial in $serials) {
                $trace = Read-NsdLog $serial
                if ($round.Name -ne 'nsd_forward' -and $trace -notmatch 'NSD-only test mode=true') { throw 'NSD-only mode was not acknowledged' }
                if ($trace -match 'LobbyService: broadcast:') { throw 'Broadcast traffic escaped NSD-only mode' }
            }
        }
        Send-MpCommand -Serial $clientDevice -Command 'lan_join_first_lobby'
        Wait-Nsd 'client joined advertised lobby' {
            Send-MpCommand -Serial $clientDevice -Command 'lan_lobby_status'
            $line = ((Read-NsdLog $clientDevice) -split "`r?`n" | Where-Object { $_ -match 'lan_lobby_status:' } | Select-Object -Last 1)
            return $line -match "joined=true game=$Game " -and $line -match 'players=2'
        }
        Send-MpCommand -Serial $clientDevice -Command 'lan_set_ready' -Extras @('--ez', 'ready', 'true')
        Wait-Nsd 'host sees both players ready' {
            Send-MpCommand -Serial $hostDevice -Command 'lan_lobby_status'
            $line = ((Read-NsdLog $hostDevice) -split "`r?`n" | Where-Object { $_ -match 'lan_lobby_status:' } | Select-Object -Last 1)
            return $line -match 'players=2' -and $line -match 'all_ready=true'
        }
        foreach ($serial in $serials) {
            Read-NsdLog $serial | Set-Content -LiteralPath (Join-Path $outDir "$($round.Name)-$serial.log")
        }
        $results.Add(@{ round = $round.Name; host = $hostDevice; client = $clientDevice; result = 'PASS' })
        Write-Status "PASS: $($round.Name) NSD confirmation, one lobby, join and ready" 'Green'
    }
} finally {
    foreach ($serial in $serials) {
        if ($stayOn.ContainsKey($serial) -and (Test-DeviceOnline -Serial $serial)) {
            try {
                Read-NsdLog $serial | Set-Content -LiteralPath (Join-Path $outDir "last-$serial.log")
            } finally {
                try {
                    Send-MpCommand -Serial $serial -Command 'lan_stop_lobby'
                    Send-MpCommand -Serial $serial -Command 'lan_nsd_only' -Extras @('--ez', 'enabled', 'false')
                } finally {
                    Adb-Dev-Timeout -Serial $serial -Seconds 5 -AdbArgs @('shell', 'settings', 'put', 'global', 'stay_on_while_plugged_in', $stayOn[$serial]) | Out-Null
                }
            }
        }
    }
    ConvertTo-Json -InputObject @($results.ToArray()) -Depth 5 | Set-Content -LiteralPath (Join-Path $outDir 'results.json')
}
