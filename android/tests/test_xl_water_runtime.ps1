#!/usr/bin/env pwsh
# Private original water fixture; requires an XL-enabled APK, retail D2 and player pilot
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [string]$Replay,
    [switch]$ReplayOnly,
    [switch]$CaptureLive,
    [switch]$LevelStart,
    [string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
if ($ReplayOnly -and !$Replay) { throw 'ReplayOnly requires Replay' }
if ($CaptureLive -and ($Replay -or $ReplayOnly)) { throw 'CaptureLive requires a separate fresh capture run' }
if ($LevelStart -and !$CaptureLive -and !$ReplayOnly) { throw 'LevelStart requires live capture or isolated replay' }
if ($Replay) {
    $replayHeader = Get-Content -LiteralPath $Replay -TotalCount 1 | ConvertFrom-Json
    $replayTrailer = Get-Content -LiteralPath $Replay -Tail 1 | ConvertFrom-Json
    if ($replayHeader.mission -ne 'rtarena' -or $replayHeader.start_mode -notin @('new_level', 'save_checkpoint') -or
        $replayHeader.level -ne 1 -or $replayHeader.frame_count -le 0 -or
        ($replayHeader.start_mode -eq 'save_checkpoint' -and
        ($replayHeader.version -ne 6 -or [string]::IsNullOrWhiteSpace($replayHeader.start_save))) -or
        $replayTrailer.type -ne 'result' -or
        (!$LevelStart -and $replayTrailer.result.position.segment -ne 187) -or
        ($LevelStart -and ($replayHeader.start_mode -ne 'new_level' -or $replayHeader.frame_count -lt 120))) {
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
$root = Split-Path (Split-Path $PSScriptRoot)
$output = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory, $root) }
else { Join-Path $root "temp/xl-water-android-$(Get-Date -Format 'yyyyMMdd-HHmmss')" }
if (Test-Path -LiteralPath $output) { throw "Water evidence output already exists: $output" }
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
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
    'files/descent.cfg', 'files/graphics_safety.json', 'files/.graphics_safety.lock', 'files/file_sets.json', 'shared_prefs')
if ($CaptureLive) { $paths += @('files/d2x-redux/input_demo_recordings', 'files/d2x-redux/xl_capture_traces') }
$ready = $false
$owned = $false
$captureExported = $false
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
        $scenario = 'test_xl_water_runtime.jsonc'
        if ($CaptureLive) {
            Invoke-WaterShell 'rm -rf files/d2x-redux/input_demo_recordings files/d2x-redux/xl_capture_traces' | Out-Null
            $scenario = 'test_xl_water_capture.jsonc'
            if ($LevelStart) { $scenario = 'test_xl_water_level_capture.jsonc' }
        }
        & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $scenario -Game d2 -Serial $Serial -PreserveState -NoEmulatorRecovery -LeaveRunning -TimeoutSeconds 240
        $testExit = $LASTEXITCODE
        Invoke-WaterShell 'cat files/automation_result.json' | Set-Content (Join-Path $output 'result.json') -Encoding utf8
        Invoke-WaterShell 'cat files/automation_log.jsonl' | Set-Content (Join-Path $output 'steps.jsonl') -Encoding utf8
        Invoke-WaterShell 'cat files/introspect.json' | Set-Content (Join-Path $output 'state.json') -Encoding utf8
        Invoke-WaterAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content (Join-Path $output 'runtime-logcat.txt') -Encoding utf8
        if ($testExit -ne 0) { throw "Android water probe failed ($testExit); see $output" }
        if ($CaptureLive) {
            $recordings = @(([string](Invoke-WaterShell 'find files/d2x-redux/input_demo_recordings/new -type f -name "*.dximdemo"')) -split '\r?\n' | Where-Object { $_ })
            $traces = @(([string](Invoke-WaterShell 'find files/d2x-redux/xl_capture_traces -type f -name "*.jsonl.gz"')) -split '\r?\n' | Where-Object { $_ })
            if ($recordings.Count -ne 1 -or $traces.Count -ne 1) { throw 'Expected one live recording and one complete observation trace' }
            if ($recordings[0] -cnotmatch '^files/d2x-redux/input_demo_recordings/new/[^/\\\x00-\x1f]+\.dximdemo$' -or
                $traces[0] -cnotmatch '^files/d2x-redux/xl_capture_traces/live-[0-9]+\.jsonl\.gz$') { throw 'Invalid capture export path' }
            foreach ($remote in @($recordings[0], ($recordings[0] + '.rngtrace.jsonl'), $traces[0])) {
                $encoded = Invoke-WaterAdb -Arguments @('exec-out', 'run-as', $script:PACKAGE, 'base64', '-w', '0', $remote)
                [IO.File]::WriteAllBytes((Join-Path $output ([IO.Path]::GetFileName($remote))), [Convert]::FromBase64String($encoded))
            }
            Write-Host 'PASS: exported Android live recording, RNG events and complete observation trace'
            $captureExported = $true
        }
    }
    if ($Replay) {
        Invoke-WaterAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
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
        . "$PSScriptRoot/../helpers/xl_android_timeline_replay.ps1"
        Invoke-XlAndroidTimelineReplay -Replay $Replay -DeviceDirectory $backup -OutputDirectory $output -WaterPhysics
    }
} finally {
    try {
        Invoke-WaterAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            if ($CaptureLive -and !$captureExported) {
                # Retain partial observations before restoring test-owned directories
                $partial = Join-Path $output 'partial-capture'
                New-Item -ItemType Directory -Force $partial | Out-Null
                $files = ([string](Invoke-WaterShell 'for p in files/d2x-redux/input_demo_recordings files/d2x-redux/xl_capture_traces; do if test -d $p; then find $p -type f; fi; done')) -split '\r?\n' | Where-Object { $_ }
                foreach ($remote in $files) {
                    if ($remote -cnotmatch '^files/d2x-redux/(input_demo_recordings|xl_capture_traces)/[A-Za-z0-9_./-]+$') { throw 'Invalid partial capture path; backup retained' }
                    $encoded = Invoke-WaterAdb -Arguments @('exec-out', 'run-as', $script:PACKAGE, 'base64', '-w', '0', $remote)
                    [IO.File]::WriteAllBytes((Join-Path $partial ($remote.Replace('/', '_'))), [Convert]::FromBase64String($encoded))
                }
            }
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
