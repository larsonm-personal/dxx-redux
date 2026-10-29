#!/usr/bin/env pwsh
# Original POG application, GPU readback and lifecycle; no campaign traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne 'adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e' -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '14150755f14e210b284aa03c45d75e1b2f96be7b567bbc324bf4986b96928a71') {
    throw 'Requires the unchanged original Robotech Arena HOG and descriptor'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-pog-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
foreach ($name in @('result.json', 'state.json', 'steps.jsonl', 'logcat.txt', 'render.png')) {
    Remove-Item -LiteralPath (Join-Path $output $name) -Force -ErrorAction SilentlyContinue
}
function Invoke-PogAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "POG adb failed: $result" }
    return $result
}
function Invoke-PogPrivate {
    param([string]$Command)
    Invoke-PogAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-pog-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/graphics_safety.json', 'files/.graphics_safety.lock',
    'files/d2x-redux/missions/rtarena.hog', 'files/d2x-redux/missions/rtarena.mn2')
$ready = $false
try {
    Invoke-PogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-PogPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-PogPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-PogPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-PogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-PogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-pog.hog') | Out-Null
    Invoke-PogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-pog.mn2') | Out-Null
    Invoke-PogPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-pog.hog files/d2x-redux/missions/rtarena.hog && cp /data/local/tmp/xl-pog.mn2 files/d2x-redux/missions/rtarena.mn2' | Out-Null
    Invoke-PogAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_pog_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-PogAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-PogPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-PogPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-PogPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "POG runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete POG runtime result' }
    Invoke-PogAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-pog.png') | Out-Null
    Invoke-PogAdb -Arguments @('pull', '/data/local/tmp/xl-pog.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Original POG GPU pixels, disk/rewind, context loss and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-PogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-PogPrivate "rm -rf $path" | Out-Null }
            Invoke-PogPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-PogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "POG test state restoration differs; backup retained at $backup" }
            Invoke-PogPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-PogAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-pog.hog', '/data/local/tmp/xl-pog.mn2', '/data/local/tmp/xl-pog.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
