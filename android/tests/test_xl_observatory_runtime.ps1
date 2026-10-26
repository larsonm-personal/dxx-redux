#!/usr/bin/env pwsh
# Original Observatory startup, rendering and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne 'e89acaad09cefa0c09fc4b487c56b6077477cfe20169f364fe54f627040b01e7' -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '8a2946c7f786f4834e1c9e303a8d4248b419b89479a750d0d6c1917fbe1b6317') {
    throw 'Requires the unchanged original Observatory HOG and descriptor'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-observatory-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-ObservatoryAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Observatory adb failed: $result" }
    return $result
}
function Invoke-ObservatoryPrivate {
    param([string]$Command)
    Invoke-ObservatoryAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-observatory-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/obsrv-xl.hog', 'files/d2x-redux/missions/obsrv-xl.mn2')
$ready = $false
try {
    Invoke-ObservatoryAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-ObservatoryPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-ObservatoryPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-ObservatoryPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-ObservatoryPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-ObservatoryAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-observatory.hog') | Out-Null
    Invoke-ObservatoryAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-observatory.mn2') | Out-Null
    Invoke-ObservatoryPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-observatory.hog files/d2x-redux/missions/obsrv-xl.hog && cp /data/local/tmp/xl-observatory.mn2 files/d2x-redux/missions/obsrv-xl.mn2' | Out-Null
    Invoke-ObservatoryAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_xl_observatory_runtime.jsonc -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 240
    $testExit = $LASTEXITCODE
    Invoke-ObservatoryAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-ObservatoryPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-ObservatoryPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-ObservatoryPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Observatory runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Observatory runtime result' }
    Invoke-ObservatoryAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-observatory.png') | Out-Null
    Invoke-ObservatoryAdb -Arguments @('pull', '/data/local/tmp/xl-observatory.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Observatory Android startup/rendering, save/restore and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-ObservatoryAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-ObservatoryPrivate "rm -rf $path" | Out-Null }
            Invoke-ObservatoryPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-ObservatoryPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Observatory test state restoration differs; backup retained at $backup" }
            Invoke-ObservatoryPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-ObservatoryAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-observatory.hog', '/data/local/tmp/xl-observatory.mn2', '/data/local/tmp/xl-observatory.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
