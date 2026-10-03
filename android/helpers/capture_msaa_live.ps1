#!/usr/bin/env pwsh
# Capture an already running debug game without resetting, launching or editing user settings
param(
    [Parameter(Mandatory)][string]$Serial,
    [ValidateSet(2, 4, 8)][int]$Samples = 2,
    [switch]$NativeMenu,
    [ValidateRange(0, 3)][int]$ScenePasses = 0
)

$ErrorActionPreference = 'Stop'
if ($NativeMenu -and $ScenePasses) { throw 'Capture native menus and scene passes separately' }
. (Join-Path $PSScriptRoot 'test_env.ps1')
. (Join-Path $PSScriptRoot 'test_helpers.ps1')
$outputDirectory = Join-Path $script:REPO_ROOT ('android/temp/msaa-live-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& (Join-Path $PSScriptRoot 'retain-recent-artifacts.ps1') -Artifacts @($outputDirectory)
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

function Invoke-CaptureDevice {
    param([string[]]$Arguments)
    $output = & $script:ADB -s $Serial @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $($Arguments -join ' '): $output" }
    return ($output -join "`n")
}

function Read-CaptureState {
    Invoke-CaptureDevice @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/introspect.json') | Out-Null
    Invoke-CaptureDevice @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.INTROSPECT') | Out-Null
    $stateTimer = [Diagnostics.Stopwatch]::StartNew()
    while ($stateTimer.Elapsed.TotalSeconds -lt 10) {
        try {
            return (Invoke-CaptureDevice @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/introspect.json') | ConvertFrom-Json)
        } catch { }
        Start-Sleep -Milliseconds 200
    }
    throw 'The running engine did not provide fresh introspection'
}

$gamePid = Invoke-CaptureDevice @('shell', 'pidof', "$($script:PACKAGE):game")
$before = Read-CaptureState
if ($before.graphics_safety.phase -ne 'idle' -or
    (-not $NativeMenu -and -not $before.game_window_is_front)) {
    throw 'Start a level and finish any graphics confirmation before capturing'
}
if ($NativeMenu -and ($before.game_window_is_front -or -not $before.menu -or $before.current_level_num -eq 0)) {
    throw 'Open a native menu over an active level before using -NativeMenu'
}
$protectedPaths = @('files/graphics_safety.json', 'files/descent.cfg', 'files/d1x-redux/descent.cfg', 'files/d2x-redux/descent.cfg')
$originalFiles = @{}
foreach ($path in $protectedPaths) {
    $originalFiles[$path] = Invoke-CaptureDevice @('shell', 'run-as', $script:PACKAGE, 'cat', $path)
}
$before | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory 'before.json') -Encoding utf8NoBOM
$scriptPath = Join-Path $outputDirectory 'msaa_live_capture.json'
$probeSteps = @(
    @{ action = 'set_debug'; field = 'msaa_color_probe'; value = "$Samples" },
    @{ action = 'assert'; expect = @{ 'msaa_probe.state_restored' = 'true' } }
)
if ($NativeMenu) {
    $probeSteps += @(
        @{ action = 'set_debug'; field = 'msaa_menu_probe'; value = '1' },
        @{ action = 'wait_for'; expect = @{ 'msaa_menu_probe.complete' = 'true' }; timeout_ms = 10000 },
        @{
            action = 'assert'
            expect = @{
                'msaa_menu_probe.after_blit.state_restored' = 'true'
                'msaa_menu_probe.before_swap.state_restored' = 'true'
            }
        }
    )
}
if ($ScenePasses) {
    $probeSteps += @(
        @{ action = 'set_debug'; field = 'msaa_scene_probe'; value = "$ScenePasses" },
        @{ action = 'wait_for'; expect = @{ 'msaa_scene_probe.complete' = 'true' }; timeout_ms = 10000 },
        @{ action = 'assert'; expect = @{ 'msaa_scene_probe.marker_state_restored' = 'true' } }
    )
}
$probeSteps | ConvertTo-Json -Depth 10 | Set-Content $scriptPath -Encoding utf8NoBOM
Invoke-CaptureDevice @('push', $scriptPath, '/data/local/tmp/msaa_live_capture.json') | Out-Null
Invoke-CaptureDevice @('shell', 'run-as', $script:PACKAGE, 'cp', '/data/local/tmp/msaa_live_capture.json', 'files/msaa_live_capture.json') | Out-Null
Invoke-CaptureDevice @('shell', 'run-as', $script:PACKAGE, 'rm', '-f', 'files/automation_result.json') | Out-Null
Invoke-CaptureDevice @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.AUTOMATE', '--es', 'script', 'msaa_live_capture.json') | Out-Null
$timer = [Diagnostics.Stopwatch]::StartNew()
$result = $null
while ($timer.Elapsed.TotalSeconds -lt 15) {
    try {
        $result = Invoke-CaptureDevice @('shell', 'run-as', $script:PACKAGE, 'cat', 'files/automation_result.json') | ConvertFrom-Json
        if ($result.result -in @('PASS', 'FAIL')) { break }
    } catch { }
    Start-Sleep -Milliseconds 200
}
$after = Read-CaptureState
$after | ConvertTo-Json -Depth 100 | Set-Content (Join-Path $outputDirectory 'after.json') -Encoding utf8NoBOM
Invoke-CaptureDevice @('logcat', '-d') | Set-Content (Join-Path $outputDirectory 'logcat.txt') -Encoding utf8NoBOM
$logNames = (Invoke-CaptureDevice @('shell', 'run-as', $script:PACKAGE, 'ls', 'files/debuglogs')) -split "`n"
foreach ($logName in $logNames) {
    $logName = $logName.Trim()
    if ($logName -match '^debuglog_[0-9]{8}_[0-9]{6}\.txt$') {
        Invoke-CaptureDevice @('shell', 'run-as', $script:PACKAGE, 'cat', "files/debuglogs/$logName") |
            Set-Content (Join-Path $outputDirectory $logName) -Encoding utf8NoBOM
    }
}
foreach ($path in $protectedPaths) {
    if ((Invoke-CaptureDevice @('shell', 'run-as', $script:PACKAGE, 'cat', $path)) -cne $originalFiles[$path]) {
        throw "Settings changed during capture: $path; evidence: $outputDirectory"
    }
}
if ((Invoke-CaptureDevice @('shell', 'pidof', "$($script:PACKAGE):game")) -ne $gamePid) {
    throw 'The game process changed during capture'
}
if ($result.result -ne 'PASS') { throw "Probe automation did not complete safely; evidence: $outputDirectory" }
Write-Output "MSAA live capture: $outputDirectory; probe passed: $($after.msaa_probe.passed); first failure: $($after.msaa_probe.first_failure)"
if ($NativeMenu) {
    Write-Output "Native menu pixels: after draw=$($after.msaa_menu_probe.after_blit.passed); before swap=$($after.msaa_menu_probe.before_swap.passed)"
}
if ($ScenePasses) {
    Write-Output "Scene pixels: passed=$($after.msaa_scene_probe.passed); passes=$($after.msaa_scene_probe.pass_count); subview=$($after.msaa_scene_probe.saw_subview)"
}
