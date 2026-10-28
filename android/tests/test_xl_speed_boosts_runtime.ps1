#!/usr/bin/env pwsh
# Derived XL speed boosts, saved motion and stock isolation; controlled poses only
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne 'b2b460804679ac074933ed2a343c50d8024b436fa76bf790e90ebf382ea34027' -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'b61c3659a8ce2076b58aaa7d995cbbb8cd045aed0dc8302a93461c09ed3ef7bb') {
    throw 'Requires the pinned derived speed boost fixture'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-speed-boosts-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-BoostAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Speed boost adb failed: $result" }
    return $result
}
function Invoke-BoostPrivate {
    param([string]$Command)
    Invoke-BoostAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-speed-boosts-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/boost.hog', 'files/d2x-redux/missions/boost.mn2')
$ready = $false
try {
    Invoke-BoostAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-BoostPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-BoostPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-BoostPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-BoostPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-BoostAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-speed-boosts.hog') | Out-Null
    Invoke-BoostAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-speed-boosts.mn2') | Out-Null
    Invoke-BoostPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-speed-boosts.hog files/d2x-redux/missions/boost.hog && cp /data/local/tmp/xl-speed-boosts.mn2 files/d2x-redux/missions/boost.mn2' | Out-Null
    Invoke-BoostAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_speed_boosts_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    $testExit = $LASTEXITCODE
    Invoke-BoostAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-BoostPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-BoostPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-BoostPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Speed boost runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete speed boost runtime result' }
    Write-Host "PASS: XL speed boost motion, authored actions, disk/rewind and stock isolation ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-BoostAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-BoostPrivate "rm -rf $path" | Out-Null }
            Invoke-BoostPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-BoostPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Speed boost restoration differs; backup retained at $backup" }
            Invoke-BoostPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-BoostAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-speed-boosts.hog', '/data/local/tmp/xl-speed-boosts.mn2') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
