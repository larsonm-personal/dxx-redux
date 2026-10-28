#!/usr/bin/env pwsh
# Original XL HXM actor, combat and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = 'f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '3ee2ab2d4175019b5b0e1db0aed8b4e0ad46d7ce4dfc09a8ac18471142a6a81b') {
    throw 'Requires the pinned original archive and descriptor from xl_hxm_native'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-hxm-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-HxmAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Hxm adb failed: $result" }
    return $result
}
function Invoke-HxmPrivate {
    param([string]$Command)
    Invoke-HxmAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-hxm-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/hxmtest.hog', 'files/d2x-redux/missions/hxmtest.mn2')
$ready = $false
try {
    Invoke-HxmAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-HxmPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-HxmPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-HxmPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-HxmPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-HxmAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-hxm.hog') | Out-Null
    Invoke-HxmAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-hxm.mn2') | Out-Null
    Invoke-HxmPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-hxm.hog files/d2x-redux/missions/hxmtest.hog && cp /data/local/tmp/xl-hxm.mn2 files/d2x-redux/missions/hxmtest.mn2' | Out-Null
    Invoke-HxmAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_hxm_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-HxmAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-HxmPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-HxmPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-HxmPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Hxm runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Hxm runtime result' }
    Invoke-HxmAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-hxm.png') | Out-Null
    Invoke-HxmAdb -Arguments @('pull', '/data/local/tmp/xl-hxm.png', "$output/render.png") | Out-Null
    Write-Host "PASS: HXM Android custom definitions/combat, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-HxmAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-HxmPrivate "rm -rf $path" | Out-Null }
            Invoke-HxmPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-HxmPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Hxm test state restoration differs; backup retained at $backup" }
            Invoke-HxmPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-HxmAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-hxm.hog', '/data/local/tmp/xl-hxm.mn2', '/data/local/tmp/xl-hxm.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
