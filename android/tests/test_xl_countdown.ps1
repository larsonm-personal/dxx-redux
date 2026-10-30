#!/usr/bin/env pwsh
# XL countdown events, HUD/audio and terminal death and disk/rewind; no traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = 'bf9bbaf3ba13345354e3539721dd86f5cdfceab4be003b2ade9f19f19cd4695e'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '52636d2e26d6f65b239b30f001af3a8f2346ce28208188f3e2537f131ac68b55') {
    throw 'Requires the pinned derived fixture from xl_countdown_runtime'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-countdown-actions-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-CountdownAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Countdown adb failed: $result" }
    return $result
}
function Invoke-CountdownPrivate {
    param([string]$Command)
    Invoke-CountdownAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-countdown-actions-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/xcount.hog', 'files/d2x-redux/missions/xcount.mn2')
$ready = $false
try {
    Invoke-CountdownAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-CountdownPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-CountdownPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-CountdownPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-CountdownPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-CountdownAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-countdown-actions.hog') | Out-Null
    Invoke-CountdownAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-countdown-actions.mn2') | Out-Null
    Invoke-CountdownPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-countdown-actions.hog files/d2x-redux/missions/xcount.hog && cp /data/local/tmp/xl-countdown-actions.mn2 files/d2x-redux/missions/xcount.mn2' | Out-Null
    Invoke-CountdownAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_countdown.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-CountdownAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-CountdownPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-CountdownPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-CountdownPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Countdown runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Countdown runtime result' }
    Invoke-CountdownAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-countdown-actions.png') | Out-Null
    Invoke-CountdownAdb -Arguments @('pull', '/data/local/tmp/xl-countdown-actions.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Countdown Android HUD/audio/countdown/death, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-CountdownAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-CountdownPrivate "rm -rf $path" | Out-Null }
            Invoke-CountdownPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-CountdownPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Countdown test state restoration differs; backup retained at $backup" }
            Invoke-CountdownPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-CountdownAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-countdown-actions.hog', '/data/local/tmp/xl-countdown-actions.mn2', '/data/local/tmp/xl-countdown-actions.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
