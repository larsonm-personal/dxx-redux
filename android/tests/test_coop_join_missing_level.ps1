#!/usr/bin/env pwsh
# Full D1 level 10 host versus seven-level demo client, followed by a matching-data retry
param(
    [ValidateSet('both', 'd1', 'd2')][string]$Game = 'both',
    [switch]$SkipInstall
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$hostSerial = $script:PRIMARY_EMULATOR_SERIAL
$clientSerial = $script:SECONDARY_EMULATOR_SERIAL
$setPath = 'files/imported/sets/default'
$backup = 'files/coop_join_level_backup'
$output = Join-Path $script:REPO_ROOT 'temp/coop_join_missing_level'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output | Out-Null
New-Item -ItemType Directory -Force -Path $output | Out-Null

function Device-Command([string]$Serial, [string[]]$Arguments) {
    $previousSerial = $env:ANDROID_SERIAL
    try {
        $env:ANDROID_SERIAL = $Serial
        $diagnostic = $null
        $result = Adb-Timeout -AdbArgs $Arguments -Seconds 20 -DiagnosticResult ([ref]$diagnostic)
        if ($diagnostic.exit_code -ne 0 -or $diagnostic.timed_out) {
            throw "Device command failed on ${Serial}: $Arguments $($diagnostic.stderr)"
        }
        return $result
    } finally {
        $env:ANDROID_SERIAL = $previousSerial
    }
}

function Start-LevelJoin([string]$Engine, [string]$Address) {
    $extras = @('--es', 'game', $Engine, '--es', 'mode', 'coop', '--ei', 'level_num', '10',
        '--ei', 'max_players', '2', '--es', 'callsign', 'LvJoin', '--es', 'mp_mode', 'join',
        '--es', 'host_addr', $Address, '--ei', 'host_port', '42424')
    if ($Engine -eq 'd2') { $extras += @('--es', 'mission', 'descent') }
    Send-MpCommand -Serial $clientSerial -Command 'lan_launch' -Extras $extras
}

function Read-LevelLobby([string]$Serial) {
    Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/mp_introspect.json') -Seconds 5 | Out-Null
    Send-MpCommand -Serial $Serial -Command 'introspect'
    $raw = Adb-Dev-Timeout -Serial $Serial -AdbArgs @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/mp_introspect.json') -Seconds 5
    try { return $raw | ConvertFrom-Json } catch { return $null }
}

function Assert-LobbyIdentity([string]$Engine, [bool]$Warning, [string]$Label) {
    if (-not (Wait-ForCondition -Description "$Engine lobby identity: $Label" -TimeoutSec 30 -Condition {
                $state = Read-LevelLobby $clientSerial
                $requirement = $state.lan.joined_mission_requirement
                $status = $state.lan.joined_mission_status
                if (-not $requirement -or -not $status) { return $false }
                $state | ConvertTo-Json -Depth 100 | Set-Content "$output/$Engine-lobby-$Label.json" -Encoding utf8
                $requirement.sha256 -eq $script:fullHogHash -and $requirement.size_bytes -eq $script:fullHogSize -and
                $status.revision -eq $requirement.revision -and $status.status -eq 'match' -and
                $status.game_data_warning -eq $Warning
            })) { throw "Lobby identity did not report $Label" }
}

foreach ($serial in @($hostSerial, $clientSerial)) {
    if ($serial -notmatch '^emulator-\d+$' -or
        (Device-Command $serial @('shell', 'getprop', 'ro.kernel.qemu')).Trim() -ne '1') {
        throw 'This test only modifies disposable emulators'
    }
    $provisioned = if ($SkipInstall) {
        Ensure-StandardGameDataOnDevice -Serial $serial
    } else {
        Install-AppAndData -Serial $serial
    }
    if (-not $provisioned) { throw "Could not provision $serial" }
}
if ($hostSerial -eq $clientSerial) { throw 'Two distinct emulators are required' }
$script:hostAddress = $null
if (-not (Wait-ForCondition -Description 'Host Wi-Fi address' -TimeoutSec 30 -Condition {
            $script:hostAddress = Get-DeviceWlanIp -Serial $hostSerial
            [bool]$script:hostAddress
        })) { throw 'Host Wi-Fi address is missing' }
$engines = if ($Game -eq 'both') { @('d1', 'd2') } else { @($Game) }
$fullData = Resolve-StandardGameDataDirectory -Candidates (Get-StandardGameDataCandidates -RepoRoot $script:REPO_ROOT -Game d1) `
    -Dependencies @((Get-StandardGameDataDeps) | Where-Object file -eq 'descent.hog') -Label 'D1 HOG identity'
if (-not $fullData) { throw 'Full D1 HOG fixture is missing' }
$fullHog = Join-Path $fullData.Path 'descent.hog'
$script:fullHogHash = (Get-FileHash $fullHog -Algorithm SHA256).Hash.ToLowerInvariant()
$script:fullHogSize = (Get-Item $fullHog).Length
$mutation = [IO.File]::ReadAllBytes($fullHog)
$mutation[$mutation.Length - 1] = $mutation[$mutation.Length - 1] -bxor 1
$mutatedHog = Join-Path $output 'mutated-descent.hog'
[IO.File]::WriteAllBytes($mutatedHog, $mutation)

foreach ($engine in $engines) {
    $saved = $false
    try {
        foreach ($serial in @($hostSerial, $clientSerial)) {
            if (-not (Start-SetupActivity -Serial $serial)) { throw "Launcher failed on $serial" }
        }
        Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'mkdir', $backup) | Out-Null
        foreach ($name in @('descent.hog', 'descent.pig')) {
            Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'cp', "$setPath/$name", "$backup/$name") | Out-Null
        }
        $saved = $true
        foreach ($name in @('descent.hog', 'descent.pig')) {
            $source = Join-Path $script:REPO_ROOT "game_data/shareware/d1/$name"
            Device-Command $clientSerial @('push', $source, "/data/local/tmp/join-level-$name") | Out-Null
            Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/join-level-$name", "$setPath/$name") | Out-Null
        }
        $missionArgument = if ($engine -eq 'd2') { 'descent' } else { "''" }
        Send-MpCommand -Serial $hostSerial -Command 'lan_host_lobby' -Extras @('--es', 'callsign', 'LvHost', '--es', 'game', $engine, '--es', 'mission', $missionArgument)
        Send-MpCommand -Serial $clientSerial -Command 'lan_discover' -Extras @('--es', 'callsign', 'LvJoin')
        if (-not (Wait-ForCondition -Description "$engine joins launcher lobby" -TimeoutSec 30 -Condition {
                    Send-MpCommand -Serial $clientSerial -Command 'lan_join_first_lobby' -Extras @('--es', 'host_addr', $hostAddress)
                    $state = Read-LevelLobby $clientSerial
                    $state.lan.joined_host -eq $hostAddress
                })) { throw 'Client did not join the launcher lobby' }
        Assert-LobbyIdentity $engine $true 'demo-mismatch'
        Send-MpCommand -Serial $clientSerial -Command 'lan_set_ready' -Extras @('--ez', 'ready', 'true')
        if (-not (Wait-ForCondition -Description "$engine mismatch remains advisory" -TimeoutSec 15 -Condition {
                    $state = Read-LevelLobby $hostSerial
                    @($state.lan.players | Where-Object { $_.callsign -eq 'LvJoin' -and $_.ready }).Count -eq 1
                })) { throw 'Built-in data warning disabled Ready' }
        Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'cp', "$backup/descent.hog", "$setPath/descent.hog") | Out-Null
        Assert-LobbyIdentity $engine $false 'matching'
        Device-Command $clientSerial @('push', $mutatedHog, '/data/local/tmp/mutated-descent.hog') | Out-Null
        Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/mutated-descent.hog', "$setPath/descent.hog") | Out-Null
        Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'touch', '-t', '203001010000.00', "$setPath/descent.hog") | Out-Null
        Assert-LobbyIdentity $engine $true 'same-size-hash-mismatch'
        Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'cp', "$backup/descent.hog", "$setPath/descent.hog") | Out-Null
        Assert-LobbyIdentity $engine $false 'restored'
        Send-MpCommand -Serial $clientSerial -Command 'lan_leave_lobby'
        Send-MpCommand -Serial $hostSerial -Command 'lan_stop_lobby'
        Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/join-level-descent.hog', "$setPath/descent.hog") | Out-Null
        $hostExtras = @('--es', 'game', $engine, '--es', 'mode', 'coop', '--ei', 'level_num', '10',
            '--ei', 'max_players', '2', '--es', 'callsign', 'LvHost', '--es', 'mp_mode', 'host')
        if ($engine -eq 'd2') { $hostExtras += @('--es', 'mission', 'descent') }
        Send-MpCommand -Serial $hostSerial -Command 'lan_launch' -Extras $hostExtras
        if (-not (Wait-ForCondition -Description "$engine host network lobby" -TimeoutSec 180 -Condition {
                    $intro = Get-GameIntrospection -Serial $hostSerial -Fresh
                    $intro -and $intro.is_network -and $intro.multiplayer.num_connected -eq 1
                })) { throw 'Host network lobby did not open' }

        Start-LevelJoin $engine $hostAddress
        $script:rejected = $null
        if (-not (Wait-ForCondition -Description "$engine missing-level error" -TimeoutSec 120 -Condition {
                    $script:rejected = Get-GameIntrospection -Serial $clientSerial -Fresh
                    $script:rejected -and $script:rejected.menu.subtitle -match 'host level 10 is not installed' -and
                    $script:rejected.menu.subtitle -match 'levels 1-7' -and -not $script:rejected.is_network
                })) { throw 'Client did not show the missing-level error' }
        $script:rejected | ConvertTo-Json -Depth 100 | Set-Content "$output/$engine-error.json" -Encoding utf8
        $before = Device-Command $clientSerial @('shell', 'pidof', "$($script:PACKAGE):game")
        $dismiss = 'test_coop_join_missing_level_dismiss.jsonc'
        $dismissPath = & {
            Set-StrictMode -Off
            Resolve-TestScript -ScriptPath "$PSScriptRoot/../game_scripts/$dismiss" -GameId $engine
        }
        Device-Command $clientSerial @('push', $dismissPath, "/data/local/tmp/$dismiss") | Out-Null
        Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'cp', "/data/local/tmp/$dismiss", "files/$dismiss") | Out-Null
        Device-Command $clientSerial @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '-p', $script:PACKAGE,
            '--es', 'script', $dismiss) | Out-Null
        if (-not (Wait-ForCondition -Description "$engine returns to menu" -TimeoutSec 30 -Condition {
                    $intro = Get-GameIntrospection -Serial $clientSerial -Fresh
                    $intro -and $intro.menu.num_items -gt 1 -and -not $intro.is_network
                })) { throw 'Error could not be dismissed' }
        $after = Device-Command $clientSerial @('shell', 'pidof', "$($script:PACKAGE):game")
        if ($before -ne $after) { throw 'Client process restarted after dismissing error' }
        $log = Device-Command $clientSerial @('logcat', '-d')
        $log | Set-Content "$output/$engine-logcat.txt" -Encoding utf8
        if ($log -match 'ASSERT FAIL|Fatal signal|Abort message:') { throw 'Client crashed during rejection' }

        Device-Command $clientSerial @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        foreach ($name in @('descent.hog', 'descent.pig')) {
            Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'cp', "$backup/$name", "$setPath/$name") | Out-Null
        }
        if (-not (Start-SetupActivity -Serial $clientSerial)) { throw 'Retry launcher failed' }
        Start-LevelJoin $engine $hostAddress
        if (-not (Wait-ForCondition -Description "$engine matching-data retry enters level 10" -TimeoutSec 90 -Condition {
                    $intro = Get-GameIntrospection -Serial $clientSerial -Fresh
                    $intro -and $intro.is_network -and $intro.current_level_num -eq 10 -and $intro.multiplayer.num_connected -eq 2
                })) { throw 'Matching-data retry failed to join level 10' }
        Write-Host "PASS: $engine rejects unavailable level, returns to menu alive, and joins after importing matching data"
    } finally {
        foreach ($serial in @($hostSerial, $clientSerial)) {
            try {
                Device-Command $serial @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
            } catch {
                Write-Warning $_
            }
        }
        if ($saved) {
            foreach ($name in @('descent.hog', 'descent.pig')) {
                Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'cp', "$backup/$name", "$setPath/$name") | Out-Null
                Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'rm', "$backup/$name") | Out-Null
            }
            Device-Command $clientSerial @('shell', 'run-as', $script:PACKAGE, 'rmdir', $backup) | Out-Null
        }
    }
}
