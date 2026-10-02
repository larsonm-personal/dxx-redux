#!/usr/bin/env pwsh
# Verify real Android touch coordinates after full accepted-mode rollback
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [string]$Serial = 'emulator-5554',
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
if ($Serial -notmatch '^emulator-') { throw 'This fixture resets game state and requires a provisioned emulator' }
. (Join-Path $PSScriptRoot 'test_env.ps1')
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
$outputDirectory = Join-Path $script:REPO_ROOT ('android/temp/graphics-restored-touch-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($outputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$games = if ($Game) { @($Game) } else { @('d1', 'd2') }
$previousSerial = $env:ANDROID_SERIAL
$env:ANDROID_SERIAL = $Serial
$results = @()

function Invoke-TouchDevice {
    param([string[]]$Arguments)
    $output = & $script:ADB -s $Serial @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $($Arguments -join ' '): $output" }
    return ($output -join "`n")
}

function Read-TouchState {
    Invoke-TouchDevice @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/introspect.json', 'files/introspect_ui.json') | Out-Null
    Invoke-TouchDevice @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT') | Out-Null
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 10) {
        try {
            $native = Invoke-TouchDevice @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json') | ConvertFrom-Json
            $ui = Invoke-TouchDevice @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect_ui.json') | ConvertFrom-Json
            return @{ native = $native; ui = $ui }
        } catch { }
        Start-Sleep -Milliseconds 100
    }
    throw 'Fresh engine/UI introspection timed out'
}

function Invoke-TouchAutomation {
    param([object[]]$Steps)
    $path = Join-Path $outputDirectory 'touch_steps.json'
    ConvertTo-Json -InputObject $Steps -Depth 20 | Set-Content $path -Encoding utf8NoBOM
    Invoke-TouchDevice @('push', $path, '/data/local/tmp/touch_steps.json') | Out-Null
    Invoke-TouchDevice @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/touch_steps.json', 'files/touch_steps.json') | Out-Null
    Invoke-TouchDevice @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/automation_result.json') | Out-Null
    Invoke-TouchDevice @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', 'touch_steps.json') | Out-Null
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt 20) {
        $result = $null
        try { $result = Invoke-TouchDevice @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/automation_result.json') | ConvertFrom-Json } catch { }
        if ($result.result -eq 'FAIL') { throw "Touch fixture automation failed: $($result | ConvertTo-Json -Compress)" }
        if ($result.result -eq 'PASS') { return }
        Start-Sleep -Milliseconds 100
    }
    throw 'Touch fixture automation timed out'
}

try {
    foreach ($gameId in $games) {
        Invoke-TouchDevice @('logcat', '-c') | Out-Null
        $arguments = @('-NoProfile', '-File', (Join-Path $PSScriptRoot 'run_test.ps1'), '-ScriptName', 'test_graphics_mode_restore.jsonc', '-Game', $gameId, '-LeaveRunning', '-TimeoutSeconds', '180')
        if ($Install) { $arguments += '-Install'; $Install = $false }
        & pwsh @arguments *> (Join-Path $outputDirectory "$gameId-fixture.txt")
        $fixtureExit = $LASTEXITCODE
        Invoke-TouchDevice @('logcat', '-d') | Set-Content (Join-Path $outputDirectory "$gameId-fixture-logcat.txt") -Encoding utf8NoBOM
        if ($fixtureExit -ne 0) { throw "$gameId mode-restore fixture failed" }
        $gamePid = Invoke-TouchDevice @('shell', 'pidof', "$($script:PACKAGE):game")
        Invoke-TouchAutomation @(
            @{ action = 'set_debug'; field = 'graphics_queue_option'; value = 'tex_filt:2' },
            @{ action = 'wait_for'; expect = @{ 'graphics_safety.phase' = 'challenge' }; timeout_ms = 10000 },
            @{ action = 'controller_input'; key = 'B'; pressed = $true; post_delay_ms = 50 },
            @{ action = 'controller_input'; key = 'B'; pressed = $false; post_delay_ms = 50 },
            @{ action = 'wait_for'; expect = @{ 'graphics_safety.phase' = 'idle'; 'resolution.render_width' = '640'; 'resolution.render_height' = '480' }; timeout_ms = 10000 },
            @{ action = 'key'; key = 'f2'; post_delay_ms = 200 }
        )
        foreach ($zoom in @(1000, 1100)) {
            Invoke-TouchAutomation @(@{ action = 'set_menu_viewport'; zoom_milli = $zoom; pan_milli = 0; post_delay_ms = 200 })
            $before = Read-TouchState
            $before | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "$gameId-$zoom-before.json") -Encoding utf8NoBOM
            $item = @($before.native.menu.items | Where-Object text -eq 'Graphics Options...')
            if ($item.Count -ne 1) { throw 'Expected exactly one Graphics Options item' }
            $logicalX = $item[0].x + $item[0].w / 2.0
            $logicalY = $item[0].y + $item[0].h / 2.0
            $scale = $before.native.menu_scale
            if ($scale.active) {
                $logicalX = $scale.dst.x + ($logicalX - $scale.src.x) * $scale.dst.w / $scale.src.w
                $logicalY = $scale.dst.y + ($logicalY - $scale.src.y) * $scale.dst.h / $scale.src.h
            }
            $surface = $before.ui.game_surface
            if ($surface.width -le 0 -or $surface.height -le 0) { throw 'Missing actual Android surface bounds' }
            $tapX = [int][Math]::Round($surface.x + $logicalX * $surface.width / $before.native.resolution.render_width)
            $tapY = [int][Math]::Round($surface.y + $logicalY * $surface.height / $before.native.resolution.render_height)
            Invoke-TouchDevice @('shell', 'input', 'tap', "$tapX", "$tapY") | Out-Null
            $timer = [Diagnostics.Stopwatch]::StartNew()
            do {
                $after = Read-TouchState
                if ($after.native.menu.subtitle -eq 'Graphics Options') { break }
                Start-Sleep -Milliseconds 100
            } while ($timer.Elapsed.TotalSeconds -lt 5)
            $after | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory "$gameId-$zoom-after.json") -Encoding utf8NoBOM
            if ($after.native.menu.subtitle -ne 'Graphics Options') { throw "$gameId zoom=$zoom tap at $tapX,$tapY did not open Graphics Options" }
            if ((Invoke-TouchDevice @('shell', 'pidof', "$($script:PACKAGE):game")) -ne $gamePid) { throw 'Rollback replaced the game process' }
            $results += @{ game = $gameId; zoom = $zoom; tap = @($tapX, $tapY); passed = $true; pid = $gamePid }
            ConvertTo-Json -InputObject $results -Depth 10 | Set-Content (Join-Path $outputDirectory 'results.json') -Encoding utf8NoBOM
            Invoke-TouchAutomation @(@{ action = 'key'; key = 'escape'; post_delay_ms = 200 })
        }
        Invoke-TouchDevice @('logcat', '-d') | Set-Content (Join-Path $outputDirectory "$gameId-logcat.txt") -Encoding utf8NoBOM
        Invoke-TouchDevice @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    }
} finally {
    if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial }
    else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
}
Write-Output "Restored touch tests passed: $outputDirectory"
