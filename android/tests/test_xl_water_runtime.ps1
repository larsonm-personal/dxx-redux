#!/usr/bin/env pwsh
# Private original water fixture; requires an XL-enabled APK, retail D2 and player pilot
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [string]$Replay,
    [switch]$ReplayOnly
)
$ErrorActionPreference = 'Stop'
if ($ReplayOnly -and !$Replay) { throw 'ReplayOnly requires Replay' }
if ($Replay) {
    $replayHeader = Get-Content -LiteralPath $Replay -TotalCount 1 | ConvertFrom-Json
    $replayTrailer = Get-Content -LiteralPath $Replay -Tail 1 | ConvertFrom-Json
    if ($replayHeader.mission -ne 'rtarena' -or $replayHeader.start_mode -ne 'new_level' -or
        $replayHeader.level -ne 1 -or $replayHeader.frame_count -le 0 -or
        $replayTrailer.type -ne 'result' -or $replayTrailer.result.position.segment -ne 187) {
        throw 'Requires the native original-arena new-level spawn-to-water recording'
    }
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne 'adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e' -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '14150755f14e210b284aa03c45d75e1b2f96be7b567bbc324bf4986b96928a71') {
    throw 'Requires the audited original Robotech HOG and descriptor'
}
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$root = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $root 'temp/xl-water-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null

function Invoke-WaterAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Water fixture adb failed: $result" }
    return $result
}
function Invoke-WaterShell {
    param([string]$Command)
    Invoke-WaterAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}

# A fixed exclusive directory makes interrupted runs visible, never silently overwritten
$backup = 'files/xl-water-runtime-backup'
$paths = @('Players', 'descent.cfg', 'd2x.ini', 'mod_usage.json', 'missions/rtarena.hog', 'missions/rtarena.mn2')
$ready = $false
$owned = $false
try {
    Invoke-WaterAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-WaterShell "mkdir $backup" | Out-Null
    $owned = $true
    $existing = @()
    foreach ($path in $paths) {
        $present = Invoke-WaterShell "if test -e files/d2x-redux/$path; then echo yes; fi"
        if (([string]$present).Trim() -eq 'yes') { $existing += $path }
    }
    if ($existing.Count -eq 0) { throw 'No existing game state; provision retail D2 and player pilot first' }
    Invoke-WaterShell "tar -cf $backup/state.tar -C files/d2x-redux $($existing -join ' ')" | Out-Null
    $ready = $true
    Invoke-WaterShell "mkdir $backup/mount-probe $backup/mount-probe/tail && printf 'name=test' > $backup/mount-probe/test.mn2" | Out-Null
    Invoke-WaterAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-water-runtime.hog') | Out-Null
    Invoke-WaterAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-water-runtime.mn2') | Out-Null
    Invoke-WaterShell 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-water-runtime.hog files/d2x-redux/missions/rtarena.hog && cp /data/local/tmp/xl-water-runtime.mn2 files/d2x-redux/missions/rtarena.mn2' | Out-Null
    Invoke-WaterAdb -Arguments @('logcat', '-c') | Out-Null
    if (!$ReplayOnly) {
        & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_xl_water_runtime.jsonc -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 240
        $testExit = $LASTEXITCODE
        Invoke-WaterShell 'cat files/automation_result.json' | Set-Content (Join-Path $output 'result.json') -Encoding utf8
        Invoke-WaterShell 'cat files/automation_log.jsonl' | Set-Content (Join-Path $output 'steps.jsonl') -Encoding utf8
        Invoke-WaterShell 'cat files/introspect.json' | Set-Content (Join-Path $output 'state.json') -Encoding utf8
        if ($testExit -ne 0) { throw "Android water probe failed ($testExit); see $output" }
    }
    if ($Replay) {
        Invoke-WaterAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        $appRoot = ([string](Invoke-WaterAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'pwd'))).Trim()
        $deviceReplay = "$appRoot/$backup/spawn.dximdemo"
        $deviceResult = "$deviceReplay.actual.json"
        Invoke-WaterAdb -Arguments @('push', (Resolve-Path -LiteralPath $Replay).Path, '/data/local/tmp/xl-water-spawn.dximdemo') | Out-Null
        Invoke-WaterShell "cp /data/local/tmp/xl-water-spawn.dximdemo $backup/spawn.dximdemo" | Out-Null
        Invoke-WaterAdb -Arguments @('logcat', '-c') | Out-Null
        Invoke-WaterAdb -Arguments @('shell', 'am', 'start', '-n', "$script:PACKAGE/.SetupActivity", '--es', 'input_demo_replay', $deviceReplay, '--ez', 'xl_water_physics', 'true') | Out-Null
        if (!(Wait-SetupActivityReady)) { throw 'Replay launcher did not become ready' }
        Invoke-WaterAdb -Arguments @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '--es', 'command', 'launch', '--es', 'game', 'd2') | Out-Null
        $deadline = [DateTime]::UtcNow.AddSeconds(120)
        $actualText = ''
        $sawReplayProcess = $false
        while ([DateTime]::UtcNow -lt $deadline) {
            $actualText = [string](Invoke-WaterShell "if test -s $deviceResult; then cat $deviceResult; fi")
            if ($actualText.Trim()) { break }
            $running = [string](Invoke-WaterAdb -Arguments @('shell', 'sh', '-c', "'if pidof $($script:PACKAGE):game >/dev/null; then echo running; fi'"))
            if ($running.Trim()) { $sawReplayProcess = $true }
            elseif ($sawReplayProcess) { break }
            Start-Sleep -Seconds 1
        }
        Invoke-WaterAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content (Join-Path $output 'spawn-replay-logcat.txt') -Encoding utf8
        Invoke-WaterShell "if test -f $backup/ai_schedule_probe.log; then cat $backup/ai_schedule_probe.log; fi" |
            Set-Content (Join-Path $output 'spawn-replay-ai-schedule.log') -Encoding utf8
        if (!$actualText.Trim()) { throw 'Android spawn replay exited or timed out without a result; inspect spawn-replay-logcat.txt' }
        $actualText | Set-Content (Join-Path $output 'spawn-replay-actual.json') -Encoding utf8
        $expectedText = $replayTrailer.result | ConvertTo-Json -Depth 30
        $expectedText | Set-Content (Join-Path $output 'spawn-replay-expected.json') -Encoding utf8
        if (![System.Text.Json.Nodes.JsonNode]::DeepEquals(
                [System.Text.Json.Nodes.JsonNode]::Parse($expectedText),
                [System.Text.Json.Nodes.JsonNode]::Parse($actualText))) {
            throw "Android spawn replay differs from native result; see $output"
        }
        Write-Host "PASS: Android original spawn-to-water input replay matches native result ($($replayHeader.frame_count) frames)"
    }
} finally {
    try {
        Invoke-WaterAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Exact test-owned paths only; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-WaterShell "rm -rf files/d2x-redux/$path" | Out-Null }
            Invoke-WaterShell "tar -xf $backup/state.tar -C files/d2x-redux" | Out-Null
            Invoke-WaterShell "rm -f $backup/mount-probe/test.mn2 && rmdir $backup/mount-probe/tail $backup/mount-probe && rm -f $backup/spawn.dximdemo $backup/spawn.dximdemo.actual.json $backup/ai_schedule_probe.log && rm $backup/state.tar && rmdir $backup" | Out-Null
        }
        if ($owned) {
            Invoke-WaterAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-water-runtime.hog', '/data/local/tmp/xl-water-runtime.mn2', '/data/local/tmp/xl-water-spawn.dximdemo') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
