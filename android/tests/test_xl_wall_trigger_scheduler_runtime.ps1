#!/usr/bin/env pwsh
# Derived delayed wall actions, stateful flags and pending persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '4d7a893899f28ffca4e8342e551075ab007399720256c2a1c60c75087755a473'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '5f4cfed456b34db7c78b9db0d743f4f72c54b37e5b133db32f7b7756ee8710db') {
    throw 'Requires the pinned derived fixture from xl_wall_trigger_scheduler'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-wall-trigger-scheduler-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-TriggerSchedulerAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "TriggerScheduler adb failed: $result" }
    return $result
}
function Invoke-TriggerSchedulerPrivate {
    param([string]$Command)
    Invoke-TriggerSchedulerAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-trigger_scheduler-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/timing.hog', 'files/d2x-redux/missions/timing.mn2')
$ready = $false
try {
    Invoke-TriggerSchedulerAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-TriggerSchedulerPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-TriggerSchedulerPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-TriggerSchedulerPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-TriggerSchedulerPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-TriggerSchedulerAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-trigger_scheduler.hog') | Out-Null
    Invoke-TriggerSchedulerAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-trigger_scheduler.mn2') | Out-Null
    Invoke-TriggerSchedulerPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-trigger_scheduler.hog files/d2x-redux/missions/timing.hog && cp /data/local/tmp/xl-trigger_scheduler.mn2 files/d2x-redux/missions/timing.mn2' | Out-Null
    Invoke-TriggerSchedulerAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_wall_trigger_scheduler_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    $testExit = $LASTEXITCODE
    Invoke-TriggerSchedulerAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-TriggerSchedulerPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-TriggerSchedulerPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-TriggerSchedulerPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "TriggerScheduler runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete TriggerScheduler runtime result' }
    Invoke-TriggerSchedulerAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-trigger_scheduler.png') | Out-Null
    Invoke-TriggerSchedulerAdb -Arguments @('pull', '/data/local/tmp/xl-trigger_scheduler.png', "$output/render.png") | Out-Null
    Write-Host "PASS: TriggerScheduler Android startup/rendering, save/restore and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-TriggerSchedulerAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-TriggerSchedulerPrivate "rm -rf $path" | Out-Null }
            Invoke-TriggerSchedulerPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-TriggerSchedulerPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "TriggerScheduler test state restoration differs; backup retained at $backup" }
            Invoke-TriggerSchedulerPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-TriggerSchedulerAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-trigger_scheduler.hog', '/data/local/tmp/xl-trigger_scheduler.mn2', '/data/local/tmp/xl-trigger_scheduler.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
