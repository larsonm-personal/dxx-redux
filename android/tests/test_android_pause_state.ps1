#!/usr/bin/env pwsh
# Exercise Kotlin/native pause reconciliation and engine recovery in both games
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [string]$Serial = 'emulator-5554',
    [switch]$Install
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget -Serial $Serial | Out-Null
Assert-IsolatedPhysicalTestApp
$outputDirectory = Join-Path $script:REPO_ROOT ('android/temp/pause-state-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& (Join-Path $PSScriptRoot '../helpers/retain-recent-artifacts.ps1') -Artifacts @($outputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$games = if ($Game) { @($Game) } else { @('d1', 'd2') }
$results = @()

function Device {
    param([string[]]$Arguments)
    $output = & $script:ADB -s $Serial @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $($Arguments -join ' '): $output" }
    return ($output -join "`n")
}
function State {
    Device @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/introspect.json', 'files/introspect_ui.json') | Out-Null
    Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT', '-p', $script:PACKAGE) | Out-Null
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 10) {
        try {
            $native = Device @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json') | ConvertFrom-Json
            $ui = Device @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect_ui.json') | ConvertFrom-Json
            return @{ native = $native; ui = $ui }
        } catch { Start-Sleep -Milliseconds 100 }
    }
    throw 'Fresh engine/UI introspection timed out'
}
function Wait-State {
    param([scriptblock]$Condition, [string]$Description)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    do {
        $state = State
        if (& $Condition $state) { return $state }
        Start-Sleep -Milliseconds 100
    } while ($timer.Elapsed.TotalSeconds -lt 10)
    $state | ConvertTo-Json -Depth 40 | Set-Content (Join-Path $outputDirectory 'failure-state.json')
    throw "Timed out: $Description"
}
function Ui {
    param([string]$Action)
    Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.GAME_COMMAND', '-p', $script:PACKAGE,
        '--es', 'command', 'pause_ui', '--es', 'value', $Action) | Out-Null
}
function Automate {
    param([object[]]$Steps)
    $path = Join-Path $outputDirectory 'steps.json'
    ConvertTo-Json -InputObject $Steps -Depth 20 | Set-Content $path -Encoding utf8NoBOM
    Device @('push', $path, '/data/local/tmp/pause_steps.json') | Out-Null
    Device @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/pause_steps.json', 'files/pause_steps.json') | Out-Null
    Device @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/automation_result.json') | Out-Null
    Device @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '-p', $script:PACKAGE, '--es', 'script', 'pause_steps.json') | Out-Null
    $timer = [Diagnostics.Stopwatch]::StartNew()
    do {
        $result = $null
        try { $result = Device @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/automation_result.json') | ConvertFrom-Json } catch { }
        if ($result.result -eq 'FAIL') { throw "Pause automation failed: $($result | ConvertTo-Json -Compress)" }
        if ($result.result -eq 'PASS') { return }
        Start-Sleep -Milliseconds 100
    } while ($timer.Elapsed.TotalSeconds -lt 25)
    throw 'Pause automation timed out'
}
function Running {
    Wait-State { param($s) $s.native.pause.input_allowed -and $s.ui.pause.input_allowed -and
        -not $s.native.pause.simulation_paused -and -not $s.ui.pause.simulation_paused } 'game and overlay agree on running state'
}
function Paused {
    Wait-State { param($s) $s.native.pause.simulation_paused -and $s.ui.pause.simulation_paused -and
        -not $s.native.pause.input_allowed -and -not $s.ui.pause.input_allowed } 'game and overlay agree on paused state'
}
try {
    foreach ($gameId in $games) {
        Device @('logcat', '-c') | Out-Null
        $arguments = @('-NoProfile', '-File', (Join-Path $PSScriptRoot '../helpers/run_test.ps1'),
            '-ScriptName', 'test_android_saveload_dispatch_unified.jsonc', '-Game', $gameId, '-Serial', $Serial, '-LeaveRunning')
        if ($Install) { $arguments += '-Install'; $Install = $false }
        & pwsh @arguments *> (Join-Path $outputDirectory "$gameId-fixture.txt")
        if ($LASTEXITCODE -ne 0) { throw "$gameId save/load fixture failed" }
        Running | Out-Null
        Ui 'tray_open'
        $paused = Paused
        if (-not $paused.ui.pause.can_resume) { throw 'User overlay pause must be resumable' }
        $time = $paused.native.idle_saver.game_time
        Start-Sleep -Milliseconds 500
        $later = State
        if ($later.native.idle_saver.game_time -ne $time) { throw 'Simulation advanced while paused' }
        Ui 'music_open'
        Ui 'tray_close'
        $paused = Paused
        if (-not $paused.ui.music_panel_open) { throw 'Overlapping music panel disappeared' }
        Ui 'missed_music_close'
        Running | Out-Null
        Ui 'quick_load_open'
        Paused | Out-Null
        Ui 'quick_load_close'
        Running | Out-Null
        Automate @(@{ action = 'set_debug'; field = 'android_game_request'; value = 'pause'; post_delay_ms = 200 })
        $paused = Paused
        if (-not $paused.ui.pause.can_resume -or ($paused.native.pause.reasons -band 4) -eq 0) { throw 'Native user pause missing from overlay' }
        Ui 'resume'
        $running = Running
        if ($running.ui.pause.request -eq 0 -or $running.native.pause.result -ne 1) { throw 'Resume was not acknowledged' }
        Ui 'resume'
        Running | Out-Null
        Ui 'tray_open'
        Paused | Out-Null
        Ui 'save'
        Wait-State { param($s) $s.native.menu.subtitle -eq 'Save Game' -and -not $s.ui.pause.can_resume } 'UI-to-native save menu handoff' | Out-Null
        Automate @(
            @{ action = 'key'; key = 'escape'; post_delay_ms = 150 },
            @{ action = 'key'; key = 'escape'; post_delay_ms = 200 }
        )
        Running | Out-Null
        $before = State
        Automate @(
            @{ action = 'set_debug'; field = 'android_game_request'; value = 'pause_stale_probe' },
            @{ action = 'set_debug'; field = 'android_game_request'; value = 'pause_operation_probe' },
            @{ action = 'set_debug'; field = 'android_game_request'; value = 'orphan_pause'; post_delay_ms = 300 },
            @{ action = 'assert'; expect = @{ time_paused = 'false'; 'pause.input_allowed' = 'true'; 'pause.legacy_depth' = '0' } }
        )
        $recovered = Running
        if ($recovered.native.pause.repairs -le $before.native.pause.repairs) { throw 'Orphaned pause was not diagnosed and repaired' }
        Automate @(
            @{ action = 'set_debug'; field = 'graphics_queue_option'; value = 'tex_filt:2' },
            @{ action = 'wait_for'; expect = @{ 'graphics_safety.phase' = 'challenge' }; timeout_ms = 10000 }
        )
        $graphics = Paused
        if (($graphics.native.pause.reasons -band 8) -eq 0 -or $graphics.ui.pause.can_resume) { throw 'Graphics confirmation must block Resume' }
        Ui 'resume'
        Wait-State { param($s) $s.native.pause.result -eq 2 -and $s.native.pause.simulation_paused } 'Resume rejected during graphics confirmation' | Out-Null
        Automate @(
            @{ action = 'controller_input'; key = 'B'; pressed = $true; post_delay_ms = 50 },
            @{ action = 'controller_input'; key = 'B'; pressed = $false; post_delay_ms = 300 }
        )
        Running | Out-Null
        $results += @{ game = $gameId; result = 'PASS' }
        Device @('logcat', '-d') | Set-Content (Join-Path $outputDirectory "$gameId-logcat.txt") -Encoding utf8NoBOM
        Write-Host "$gameId pause state PASS"
    }
    $results | ConvertTo-Json | Set-Content (Join-Path $outputDirectory 'results.json') -Encoding utf8NoBOM
} catch {
    Device @('logcat', '-d') | Set-Content (Join-Path $outputDirectory 'failure-logcat.txt') -Encoding utf8NoBOM
    throw
}
