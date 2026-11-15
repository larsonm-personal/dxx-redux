# Shared native Android XL playback; callers own and restore the protected directory
function Invoke-XlAndroidTimelineReplay {
    param([string]$Replay, [string]$DeviceDirectory, [string]$OutputDirectory, [switch]$WaterPhysics)
    if ($DeviceDirectory -cnotmatch '^files/[A-Za-z0-9_-]+$') { throw 'Replay requires an owned private directory' }
    function Invoke-TimelineAdb {
        param([string[]]$Arguments)
        $diagnostic = $null
        $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
        if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Timeline replay adb failed: $result" }
        return $result
    }
    function Invoke-TimelineShell {
        param([string]$Command)
        Invoke-TimelineAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
    }
    $backup = $DeviceDirectory
    $replayHeader = Get-Content -LiteralPath $Replay -TotalCount 1 | ConvertFrom-Json
    $replayTrailer = Get-Content -LiteralPath $Replay -Tail 1 | ConvertFrom-Json
    Invoke-TimelineAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    $appRoot = ([string](Invoke-TimelineAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'pwd'))).Trim()
    $deviceReplay = "$appRoot/$backup/spawn.dximdemo"
    $deviceResult = "$deviceReplay.actual.json"
    $deviceState = "$deviceReplay.state.jsonl.gz"
    $deviceRng = "$deviceReplay.rngtrace.jsonl"
    Invoke-TimelineShell "rm -f $deviceResult $deviceState $deviceRng" | Out-Null
    Invoke-TimelineAdb -Arguments @('push', (Resolve-Path -LiteralPath $Replay).Path, '/data/local/tmp/xl-timeline-spawn.dximdemo') | Out-Null
    Invoke-TimelineShell "cp /data/local/tmp/xl-timeline-spawn.dximdemo $backup/spawn.dximdemo" | Out-Null
    Invoke-TimelineAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-timeline-spawn.dximdemo') | Out-Null
    Invoke-TimelineAdb -Arguments @('logcat', '-c') | Out-Null
    $launch = @('shell', 'am', 'start', '-n', "$script:PACKAGE/.SetupActivity", '--es', 'input_demo_replay', $deviceReplay)
    if ($WaterPhysics) { $launch += @('--ez', 'xl_water_physics', 'true') }
    Invoke-TimelineAdb -Arguments $launch | Out-Null
    if (!(Wait-SetupActivityReady)) { throw 'Replay launcher did not become ready' }
    Invoke-TimelineAdb -Arguments @('shell', 'am', 'broadcast', '-a', 'com.dxxredux.SETUP_COMMAND', '--es', 'command', 'launch', '--es', 'game', 'd2') | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds(120)
    $actualText = ''
    $sawReplayProcess = $false
    while ([DateTime]::UtcNow -lt $deadline) {
        $actualText = [string](Invoke-TimelineShell "if test -s $deviceResult; then cat $deviceResult; fi")
        if ($actualText.Trim()) { break }
        $running = [string](Invoke-TimelineAdb -Arguments @('shell', 'sh', '-c', "'if pidof $($script:PACKAGE):game >/dev/null; then echo running; fi'"))
        if ($running.Trim()) { $sawReplayProcess = $true }
        elseif ($sawReplayProcess) { break }
        Start-Sleep -Seconds 1
    }
    Invoke-TimelineAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content (Join-Path $OutputDirectory 'spawn-replay-logcat.txt') -Encoding utf8
    Invoke-TimelineShell "if test -f $backup/ai_schedule_probe.log; then cat $backup/ai_schedule_probe.log; fi" |
        Set-Content (Join-Path $OutputDirectory 'spawn-replay-ai-schedule.log') -Encoding utf8
    if ($actualText.Trim()) {
        $actualText | Set-Content (Join-Path $OutputDirectory 'spawn-replay-actual.json') -Encoding utf8
    }
    foreach ($trace in @(@{ Remote = $deviceState; Local = 'spawn-replay-state.jsonl.gz' },
            @{ Remote = $deviceRng; Local = 'spawn-replay-rng.jsonl' })) {
        if (([string](Invoke-TimelineShell "if test -f $($trace.Remote); then echo yes; fi")).Trim() -eq 'yes') {
            $encoded = Invoke-TimelineAdb -Arguments @('exec-out', 'run-as', $script:PACKAGE, 'base64', '-w', '0', $trace.Remote)
            [IO.File]::WriteAllBytes((Join-Path $OutputDirectory $trace.Local), [Convert]::FromBase64String($encoded))
        } elseif ($actualText.Trim()) {
            throw "Android replay trace missing: $($trace.Local); use the explicit XL developer probe build"
        }
    }
    if (!$actualText.Trim()) { throw 'Android spawn replay exited or timed out without a result; inspect spawn-replay-logcat.txt' }
    $expectedText = $replayTrailer.result | ConvertTo-Json -Depth 30
    $expectedText | Set-Content (Join-Path $OutputDirectory 'spawn-replay-expected.json') -Encoding utf8
    if (![System.Text.Json.Nodes.JsonNode]::DeepEquals(
            [System.Text.Json.Nodes.JsonNode]::Parse($expectedText),
            [System.Text.Json.Nodes.JsonNode]::Parse($actualText))) {
        throw "Android spawn replay differs from native result; see $OutputDirectory"
    }
    Write-Host "PASS: Android XL input replay matches native result ($($replayHeader.frame_count) frames)"
}
