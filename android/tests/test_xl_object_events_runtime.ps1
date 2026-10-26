#!/usr/bin/env pwsh
# Derived object events, owner consumption and disk/rewind persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '80a7d98f01c138716f3650f388216ffca0e45ab829b263726bb77d75f2014cd6'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '221deb90c04e3c1d8b954dce20968c1d6df9f7926d6bda4b0c69bb4b75bcae01') {
    throw 'Requires the pinned derived fixture from xl_object_events'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-object-events-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-ObjectEventsAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "ObjectEvents adb failed: $result" }
    return $result
}
function Invoke-ObjectEventsPrivate {
    param([string]$Command)
    Invoke-ObjectEventsAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-object_events-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/events.hog', 'files/d2x-redux/missions/events.mn2')
$ready = $false
try {
    Invoke-ObjectEventsAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-ObjectEventsPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-ObjectEventsPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-ObjectEventsPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-ObjectEventsPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-ObjectEventsAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-object_events.hog') | Out-Null
    Invoke-ObjectEventsAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-object_events.mn2') | Out-Null
    Invoke-ObjectEventsPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-object_events.hog files/d2x-redux/missions/events.hog && cp /data/local/tmp/xl-object_events.mn2 files/d2x-redux/missions/events.mn2' | Out-Null
    Invoke-ObjectEventsAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_object_events_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    $testExit = $LASTEXITCODE
    Invoke-ObjectEventsAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-ObjectEventsPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-ObjectEventsPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-ObjectEventsPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "ObjectEvents runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete ObjectEvents runtime result' }
    Invoke-ObjectEventsAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-object_events.png') | Out-Null
    Invoke-ObjectEventsAdb -Arguments @('pull', '/data/local/tmp/xl-object_events.png', "$output/render.png") | Out-Null
    Write-Host "PASS: ObjectEvents Android damage/removal/reactor, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-ObjectEventsAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-ObjectEventsPrivate "rm -rf $path" | Out-Null }
            Invoke-ObjectEventsPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-ObjectEventsPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "ObjectEvents test state restoration differs; backup retained at $backup" }
            Invoke-ObjectEventsPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-ObjectEventsAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-object_events.hog', '/data/local/tmp/xl-object_events.mn2', '/data/local/tmp/xl-object_events.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
