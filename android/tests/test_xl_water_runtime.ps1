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
    if ($replayHeader.mission -ne 'rtarena' -or $replayHeader.start_mode -notin @('new_level', 'save_checkpoint') -or
        $replayHeader.level -ne 1 -or $replayHeader.frame_count -le 0 -or
        ($replayHeader.start_mode -eq 'save_checkpoint' -and
        ($replayHeader.version -ne 6 -or [string]::IsNullOrWhiteSpace($replayHeader.start_save))) -or
        $replayTrailer.type -ne 'result' -or $replayTrailer.result.position.segment -ne 187) {
        throw 'Requires the native original-arena spawn-to-water recording and its supported startup profile'
    }
    if ($replayHeader.version -eq 6 -and
        ($replayHeader.view_projection.screen_width -ne 1920 -or
        $replayHeader.view_projection.screen_height -ne 1080 -or
        $replayHeader.view_projection.aspect -ne 65536)) {
        throw 'Water checkpoint qualification requires the canonical 1920x1080 square-pixel capture'
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
$output = Join-Path $root "temp/xl-water-android-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
Write-Host "Water runtime evidence: $output"

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
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/d2x-redux/missions/rtarena.hog', 'files/d2x-redux/missions/rtarena.mn2',
    'files/descent.cfg', 'files/graphics_safety.json', 'files/file_sets.json', 'shared_prefs')
$ready = $false
$owned = $false
try {
    Invoke-WaterAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-WaterShell "mkdir $backup" | Out-Null
    $owned = $true
    $existing = @()
    foreach ($path in $paths) {
        $present = Invoke-WaterShell "if test -e $path; then echo yes; fi"
        if (([string]$present).Trim() -eq 'yes') { $existing += $path }
    }
    if ($existing.Count -eq 0) { throw 'No existing game state; provision retail D2 and player pilot first' }
    Invoke-WaterShell "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = ([string](Invoke-WaterShell "find $($existing -join ' ') -type f -exec sha256sum {} \;")) -split '\r?\n' | Where-Object { $_ } | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
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
        $deviceState = "$deviceReplay.state.jsonl.gz"
        $deviceRng = "$deviceReplay.rngtrace.jsonl"
        if ($replayHeader.version -eq 6) {
            # Use the recorded viewport, not a rewritten capture or relaxed native check
            $profile = [ordered]@{ TexFilt = 0; AnisoLevel = 0; MsaaLevel = 0; MenuTexFilt = 0; HudTexFilt = 0;
                ResolutionX = 1920; ResolutionY = 1080; AspectX = 9; AspectY = 16; ColorDepth = 0
            }
            [ordered]@{ accepted = $profile; attempt = $null; pending = @{} } |
                ConvertTo-Json -Depth 4 | Set-Content "$output/replay-graphics.json" -Encoding utf8NoBOM
            Invoke-WaterAdb -Arguments @('push', "$output/replay-graphics.json", '/data/local/tmp/xl-water-graphics.json') | Out-Null
            Invoke-WaterShell 'cp /data/local/tmp/xl-water-graphics.json files/graphics_safety.json' | Out-Null
            foreach ($config in @('files/descent.cfg', 'files/d2x-redux/descent.cfg')) {
                $original = [string](Invoke-WaterShell "if test -f $config; then cat $config; fi")
                $values = @($profile.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join "`n"
                $localConfig = Join-Path $output ($config.Replace('/', '-') + '.replay')
                "$original`n$values`n" | Set-Content $localConfig -Encoding utf8NoBOM
                Invoke-WaterAdb -Arguments @('push', $localConfig, '/data/local/tmp/xl-water-config.txt') | Out-Null
                Invoke-WaterShell "cp /data/local/tmp/xl-water-config.txt $config" | Out-Null
            }
        }
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
        if ($actualText.Trim()) {
            $actualText | Set-Content (Join-Path $output 'spawn-replay-actual.json') -Encoding utf8
        }
        foreach ($trace in @(@{ Remote = $deviceState; Local = 'spawn-replay-state.jsonl.gz' },
                @{ Remote = $deviceRng; Local = 'spawn-replay-rng.jsonl' })) {
            if (([string](Invoke-WaterShell "if test -f $($trace.Remote); then echo yes; fi")).Trim() -eq 'yes') {
                $encoded = Invoke-WaterAdb -Arguments @('exec-out', 'run-as', $script:PACKAGE, 'base64', '-w', '0', $trace.Remote)
                [IO.File]::WriteAllBytes((Join-Path $output $trace.Local), [Convert]::FromBase64String($encoded))
            } elseif ($actualText.Trim()) {
                throw "Android replay trace missing: $($trace.Local); use the explicit XL developer probe build"
            }
        }
        if (!$actualText.Trim()) { throw 'Android spawn replay exited or timed out without a result; inspect spawn-replay-logcat.txt' }
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
            foreach ($path in $paths) { Invoke-WaterShell "rm -rf $path" | Out-Null }
            Invoke-WaterShell "tar -xf $backup/state.tar" | Out-Null
            $after = ([string](Invoke-WaterShell "find $($existing -join ' ') -type f -exec sha256sum {} \;")) -split '\r?\n' | Where-Object { $_ } | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Water test state restoration differs; backup retained at $backup" }
            Invoke-WaterShell "rm -f $backup/mount-probe/test.mn2 && rmdir $backup/mount-probe/tail $backup/mount-probe && rm -f $backup/spawn.dximdemo $backup/spawn.dximdemo.actual.json $backup/ai_schedule_probe.log $backup/spawn.dximdemo.state.jsonl.gz $backup/spawn.dximdemo.rngtrace.jsonl $backup/spawn.dximdemo.rngtrace.jsonl.events.tmp && rm $backup/state.tar && rmdir $backup" | Out-Null
        }
        if ($owned) {
            Invoke-WaterAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-water-runtime.hog', '/data/local/tmp/xl-water-runtime.mn2', '/data/local/tmp/xl-water-spawn.dximdemo', '/data/local/tmp/xl-water-graphics.json', '/data/local/tmp/xl-water-config.txt') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
