#!/usr/bin/env pwsh
# Derived XL blocked entry, exit, projectiles, automap and disk/rewind persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '42456e953153e828386ae8ee3cef80701dd49f527d5ecfd46392908fbbaafdd7'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '0f1a73772a8f2c2f1b4eaf287fe0e566a899cedeb568b935eeb67b3c0d616754') {
    throw 'Requires the pinned derived fixture from xl_blocked_segments'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-blocked-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-BlockedAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Blocked adb failed: $result" }
    return $result
}
function Invoke-BlockedPrivate {
    param([string]$Command)
    Invoke-BlockedAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-blocked-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/blocked.hog', 'files/d2x-redux/missions/blocked.mn2')
$ready = $false
try {
    Invoke-BlockedAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-BlockedPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-BlockedPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-BlockedPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-BlockedPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-BlockedAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-blocked.hog') | Out-Null
    Invoke-BlockedAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-blocked.mn2') | Out-Null
    Invoke-BlockedPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-blocked.hog files/d2x-redux/missions/blocked.hog && cp /data/local/tmp/xl-blocked.mn2 files/d2x-redux/missions/blocked.mn2' | Out-Null
    Invoke-BlockedAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_blocked_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-BlockedAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-BlockedPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-BlockedPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-BlockedPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Blocked runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Blocked runtime result' }
    Invoke-BlockedAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-blocked.png') | Out-Null
    Invoke-BlockedAdb -Arguments @('pull', '/data/local/tmp/xl-blocked.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Blocked Android entry/exit/projectiles/automap/production, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-BlockedAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-BlockedPrivate "rm -rf $path" | Out-Null }
            Invoke-BlockedPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-BlockedPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Blocked test state restoration differs; backup retained at $backup" }
            Invoke-BlockedPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-BlockedAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-blocked.hog', '/data/local/tmp/xl-blocked.mn2', '/data/local/tmp/xl-blocked.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
