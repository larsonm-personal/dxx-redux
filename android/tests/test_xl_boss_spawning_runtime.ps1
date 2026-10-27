#!/usr/bin/env pwsh
# Derived XL boss spawning and per-owner state and disk/rewind persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [switch]$Transport
)
$ErrorActionPreference = 'Stop'
$expectedHog = if ($Transport) { '09e5deac6470879bd501ab60f982eec79c610f5b65d0dd4afc0e4142f887c1a2' } else { '718b507ed3866a73f36fed6472810c2310d99dc1861aece09002101575c60d9c' }
$expectedDescriptor = if ($Transport) { '7a929982ead40553557c9364916cc773a15dad23e5a8d895fd142737f936fb2e' } else { '27f367855bbb7748536b29b3fbf84faf8583c216f3276a220eed2be959070ed2' }
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne $expectedDescriptor) {
    throw 'Requires the pinned derived boss spawning or transport fixture for the selected mode'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$missionBase = if ($Transport) { 'bosstran' } else { 'bosses' }
$output = Join-Path $PSScriptRoot $(if ($Transport) { '../../temp/xl-boss-transport-android' } else { '../../temp/xl-boss-spawning-android' })
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-BossSpawningAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "BossSpawning adb failed: $result" }
    return $result
}
function Invoke-BossSpawningPrivate {
    param([string]$Command)
    Invoke-BossSpawningAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = if ($Transport) { 'files/xl-boss_transport-runtime-backup' } else { 'files/xl-boss_spawning-runtime-backup' }
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    "files/d2x-redux/missions/$missionBase.hog", "files/d2x-redux/missions/$missionBase.mn2")
$ready = $false
try {
    Invoke-BossSpawningAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-BossSpawningPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-BossSpawningPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-BossSpawningPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-BossSpawningPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-BossSpawningAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-boss_spawning.hog') | Out-Null
    Invoke-BossSpawningAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-boss_spawning.mn2') | Out-Null
    Invoke-BossSpawningPrivate "mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-boss_spawning.hog files/d2x-redux/missions/$missionBase.hog && cp /data/local/tmp/xl-boss_spawning.mn2 files/d2x-redux/missions/$missionBase.mn2" | Out-Null
    Invoke-BossSpawningAdb -Arguments @('logcat', '-c') | Out-Null
    if ($Transport) {
        & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_boss_transport_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    } else {
        & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_boss_spawning_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    }
    $testExit = $LASTEXITCODE
    Invoke-BossSpawningAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-BossSpawningPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-BossSpawningPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-BossSpawningPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "BossSpawning runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete BossSpawning runtime result' }
    Invoke-BossSpawningAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-boss_spawning.png') | Out-Null
    Invoke-BossSpawningAdb -Arguments @('pull', '/data/local/tmp/xl-boss_spawning.png', "$output/render.png") | Out-Null
    if ($Transport) {
        Write-Host "PASS: BossTransport Android authored actions, owned destinations, disk/rewind and stock transition ($($result.total_steps) steps)"
    } else {
        Write-Host "PASS: BossSpawning Android activation/spawning/combat, disk/rewind and stock transition ($($result.total_steps) steps)"
    }
} finally {
    try {
        Invoke-BossSpawningAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-BossSpawningPrivate "rm -rf $path" | Out-Null }
            Invoke-BossSpawningPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-BossSpawningPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "BossSpawning test state restoration differs; backup retained at $backup" }
            Invoke-BossSpawningPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-BossSpawningAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-boss_spawning.hog', '/data/local/tmp/xl-boss_spawning.mn2', '/data/local/tmp/xl-boss_spawning.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
