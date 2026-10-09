# Loaded by test_lan.ps1 after establishing a real two-player co-op session
function Invoke-MultiplayerPauseScenario {
    $directory = Join-Path $REPO_ROOT ('android/temp/coop-pause-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    & (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts $directory | Out-Null
    New-Item -ItemType Directory -Force $directory | Out-Null

    function Invoke-PauseDevice {
        param([string]$Serial, [string[]]$Arguments)
        $output = & $ADB -s $Serial @Arguments 2>&1
        if ($LASTEXITCODE -ne 0) { throw "adb failed on ${Serial}: $output" }
        return ($output -join "`n")
    }
    function Send-PauseUi {
        param([string]$Serial, [string]$Action)
        Invoke-PauseDevice $Serial @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.GAME_COMMAND', '-p', $PACKAGE,
            '--es', 'command', 'pause_ui', '--es', 'value', $Action) | Out-Null
    }
    function Wait-PauseState {
        param([string]$Serial, [scriptblock]$Condition, [string]$Description, [int]$Timeout = 15)
        $timer = [Diagnostics.Stopwatch]::StartNew()
        do {
            $state = Get-GameIntrospection -Serial $Serial -Fresh
            if ($state -and (& $Condition $state)) { return $state }
            Start-Sleep -Milliseconds 100
        } while ($timer.Elapsed.TotalSeconds -lt $Timeout)
        $state | ConvertTo-Json -Depth 60 | Set-Content (Join-Path $directory "failure-$Serial.json")
        throw "${Serial}: $Description"
    }
    function Invoke-PauseSteps {
        param([string]$Serial, [object[]]$Steps)
        $path = Join-Path $directory 'steps.json'
        ConvertTo-Json -InputObject $Steps -Depth 20 | Set-Content $path -Encoding utf8NoBOM
        Invoke-PauseDevice $Serial @('push', $path, '/data/local/tmp/coop_pause_steps.json') | Out-Null
        Invoke-PauseDevice $Serial @('shell', 'run-as', $PACKAGE, 'cp', '/data/local/tmp/coop_pause_steps.json', 'files/coop_pause_steps.json') | Out-Null
        Invoke-PauseDevice $Serial @('shell', 'run-as', $PACKAGE, 'rm', '-f', 'files/automation_result.json') | Out-Null
        Invoke-PauseDevice $Serial @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '-p', $PACKAGE,
            '--es', 'script', 'coop_pause_steps.json') | Out-Null
        $timer = [Diagnostics.Stopwatch]::StartNew()
        do {
            $result = Get-DeviceAutomationResult -Serial $Serial
            if ($result -and $result.result -eq 'PASS') { return }
            if ($result -and $result.result -eq 'FAIL') { throw "${Serial}: $($result | ConvertTo-Json -Compress)" }
            Start-Sleep -Milliseconds 100
        } while ($timer.Elapsed.TotalSeconds -lt 25)
        throw "Automation timed out on $Serial"
    }
    function Assert-PauseLive {
        param([string]$Serial, [string]$Case)
        $first = Wait-PauseState $Serial { param($s) -not $s.pause.input_allowed } "$Case did not block local input"
        Start-Sleep -Milliseconds 500
        $second = Get-GameIntrospection -Serial $Serial -Fresh
        $ui = Invoke-PauseDevice $Serial @('shell', 'run-as', $PACKAGE, 'cat', 'files/introspect_ui.json') | ConvertFrom-Json
        $remoteSlot = if ($Serial -eq $EMU1) { 1 } else { 0 }
        @($first, $second) | ConvertTo-Json -Depth 60 | Set-Content (Join-Path $directory "$Serial-$Case.json")
        if ($second.time_paused -or $second.multiplayer.num_connected -ne 2 -or
            $second.idle_saver.game_time -le $first.idle_saver.game_time -or
            (Get-IntroPdataSequence $first $remoteSlot) -eq (Get-IntroPdataSequence $second $remoteSlot)) {
            throw "$Case interrupted co-op simulation or network traffic on $Serial"
        }
        if ($second.fire_primary_state -ne 0 -or $second.heading_time -ne 0) {
            throw "$Case retained held ship controls on ${Serial}: fire=$($second.fire_primary_state) yaw=$($second.heading_time)"
        }
        if ($ui.paused_warning_shown -or $ui.pause.simulation_paused -or $ui.pause.input_allowed) {
            throw "$Case has incorrect co-op overlay pause state on $Serial"
        }
        if (-not $second.game_window_is_front -and $ui.touch_overlay_active) {
            throw "$Case has a gameplay overlay intercepting native menu input on $Serial"
        }
    }
    function Wait-PauseRunning {
        param([string]$Serial)
        Wait-PauseState $Serial { param($s) $s.pause.input_allowed -and -not $s.time_paused -and
            $s.multiplayer.num_connected -eq 2 } 'Gameplay did not resume' | Out-Null
    }

    # Menus deliberately leave combat live; keep this navigation fixture out of combat
    foreach ($serial in @($EMU1, $EMU2)) {
        Invoke-PauseSteps $serial @(@{ action = 'set_debug'; field = 'clear_robots'; value = 'true' })
    }
    Invoke-PauseSteps $EMU2 @(@{ action = 'set_debug'; field = 'coop_death_test'; value = 'move_away' })

    foreach ($serial in @($EMU1, $EMU2)) {
        foreach ($modal in @('quick_load', 'tray', 'music')) {
            Write-Status "Checking $modal with held fire on $serial"
            Invoke-PauseSteps $serial @(
                @{ action = 'send_button'; button = 100; held = 1 },
                @{ action = 'wait_for'; expect = @{ fire_primary_state = @{ ne = 0 } }; timeout_ms = 5000 }
            )
            Send-PauseUi $serial "${modal}_open"
            Assert-PauseLive $serial $modal
            Send-PauseUi $serial "${modal}_close"
            Wait-PauseRunning $serial
            Invoke-PauseSteps $serial @(@{ action = 'send_button'; button = 100; pressed = 0 })
        }
        foreach ($menu in @('menu', 'save', 'load')) {
            Write-Status "Checking native $menu on $serial"
            Send-PauseUi $serial 'tray_open'
            Wait-PauseState $serial { param($s) -not $s.pause.input_allowed } 'Tray did not open' | Out-Null
            Send-PauseUi $serial $menu
            if ($serial -eq $EMU2 -and $menu -ne 'menu') {
                # Guests must stay in game; only the host can save or restore the session
                Wait-PauseRunning $serial
                continue
            }
            Assert-PauseLive $serial $menu
            if ($menu -eq 'menu') {
                Invoke-PauseSteps $serial @(@{ action = 'select'; text = 'Options'; timeout_ms = 5000 })
                Assert-PauseLive $serial 'options'
            }
            Invoke-PauseSteps $serial @(@{ action = 'key'; key = 'escape'; post_delay_ms = 200 })
            if ($menu -eq 'save') {
                # Save opens with its description being edited; Escape first cancels that edit
                Invoke-PauseSteps $serial @(@{ action = 'key'; key = 'escape'; post_delay_ms = 200 })
            }
            Wait-PauseRunning $serial
        }
    }

    # Save distinct inventories, then restore while the client is in a native menu
    Write-Status 'Checking host quick save/load, guest rejection, and restore from a guest menu'
    Invoke-PauseSteps $EMU1 @(
        @{ action = 'set_debug'; field = 'player_energy'; value = '123' },
        @{ action = 'set_debug'; field = 'recovery_test_inventory'; value = 'true' }
    )
    Invoke-PauseSteps $EMU2 @(
        @{ action = 'set_debug'; field = 'player_energy'; value = '145' },
        @{ action = 'set_debug'; field = 'recovery_test_inventory'; value = 'true' }
    )
    # The inventory fixture broadcasts ship status; player_energy alone only changes local state
    Wait-PauseState $EMU1 { param($s) $s.multiplayer.players[1].energy -eq 145 } 'Guest inventory did not reach host' | Out-Null
    Invoke-PauseSteps $EMU1 @(@{ action = 'set_debug'; field = 'android_game_request'; value = 'quick_save'; post_delay_ms = 500 })
    foreach ($serial in @($EMU1, $EMU2)) { Wait-PauseRunning $serial }
    Invoke-PauseSteps $EMU1 @(@{ action = 'set_debug'; field = 'player_energy'; value = '67' })
    Invoke-PauseSteps $EMU2 @(@{ action = 'set_debug'; field = 'player_energy'; value = '89' })
    # A client confirmation must not initiate a restore or strand an input blocker
    Send-PauseUi $EMU2 'quick_load_open'
    Send-PauseUi $EMU2 'quick_load_confirm'
    Wait-PauseRunning $EMU2
    $guest = Get-GameIntrospection -Serial $EMU2 -Fresh
    if ($guest.player.energy -ne 89) { throw 'Client quick load unexpectedly changed inventory' }
    Send-PauseUi $EMU2 'menu'
    Assert-PauseLive $EMU2 'menu-before-restore'
    $before = Get-GameIntrospection -Serial $EMU1 -Fresh
    Send-PauseUi $EMU1 'quick_load_open'
    Send-PauseUi $EMU1 'quick_load_confirm'
    Wait-PauseState $EMU1 { param($s) $s.pause.session -gt $before.pause.session -and $s.player.energy -eq 123 } 'Host quick load did not restore inventory' 30 | Out-Null
    Wait-PauseState $EMU2 { param($s) $s.player.energy -eq 145 } 'Client quick load did not restore inventory' 30 | Out-Null
    Wait-PauseRunning $EMU1
    Assert-PauseLive $EMU2 'menu-after-restore'
    Invoke-PauseSteps $EMU2 @(@{ action = 'key'; key = 'escape'; post_delay_ms = 200 })
    foreach ($serial in @($EMU1, $EMU2)) {
        Wait-PauseRunning $serial
        $log = Invoke-PauseDevice $serial @('logcat', '-d', '-s', 'DXX-Pause:I')
        $log | Set-Content (Join-Path $directory "$serial-handoffs.log")
        if ($log -match 'Requested action=\d+ request=\d+ revision=\d+ modals=[1-9]') {
            throw "Native action queued before its Android modal closed on $serial"
        }
    }
    Write-Status "Co-op pause/menu/quick-save/load checks passed ($Game); evidence: $directory" 'Green'
    return $true
}
