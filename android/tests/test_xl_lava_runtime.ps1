#!/usr/bin/env pwsh
# Derived XL lava damage, motion, audio and disk/rewind persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '032a983541d4eb85e06069a34b047fbd73e443f752121372092469da94c7a9d3'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'ed1d6d53e12983c69a8196419a5d8ab1ca6cd54784148fd4e054d26cdce731ad') {
    throw 'Requires the pinned derived fixture from xl_lava_volumes'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-lava-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-LavaAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Lava adb failed: $result" }
    return $result
}
function Invoke-LavaPrivate {
    param([string]$Command)
    Invoke-LavaAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-lava-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/lava.hog', 'files/d2x-redux/missions/lava.mn2')
$ready = $false
try {
    Invoke-LavaAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-LavaPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-LavaPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-LavaPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-LavaPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-LavaAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-lava.hog') | Out-Null
    Invoke-LavaAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-lava.mn2') | Out-Null
    Invoke-LavaPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-lava.hog files/d2x-redux/missions/lava.hog && cp /data/local/tmp/xl-lava.mn2 files/d2x-redux/missions/lava.mn2' | Out-Null
    Invoke-LavaAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_lava_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-LavaAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-LavaPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-LavaPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-LavaPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Lava runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Lava runtime result' }
    Invoke-LavaAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-lava.png') | Out-Null
    Invoke-LavaAdb -Arguments @('pull', '/data/local/tmp/xl-lava.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Lava Android damage/motion/audio/production, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-LavaAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-LavaPrivate "rm -rf $path" | Out-Null }
            Invoke-LavaPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-LavaPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Lava test state restoration differs; backup retained at $backup" }
            Invoke-LavaPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-LavaAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-lava.hog', '/data/local/tmp/xl-lava.mn2', '/data/local/tmp/xl-lava.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
