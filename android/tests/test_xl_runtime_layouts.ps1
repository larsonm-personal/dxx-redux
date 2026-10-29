#!/usr/bin/env pwsh
# Canonical modern layouts, resource ownership and disk/rewind; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '38caedf8321ad8de4eb3fd452dd9efc6210cf19620b75884a90de4ff283afed8'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'b75b29439f4380affafecebcB9e7800837623750811c992d95f79014ca8fb49e') {
    throw 'Requires the pinned derived fixture from xl_modern_runtime_layouts'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-runtime-layouts-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-RuntimeLayoutAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "RuntimeLayout adb failed: $result" }
    return $result
}
function Invoke-RuntimeLayoutPrivate {
    param([string]$Command)
    Invoke-RuntimeLayoutAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-runtime-layouts-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/rtlayout.hog', 'files/d2x-redux/missions/rtlayout.mn2')
$ready = $false
try {
    Invoke-RuntimeLayoutAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-RuntimeLayoutPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-RuntimeLayoutPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-RuntimeLayoutPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-RuntimeLayoutPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-RuntimeLayoutAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-runtime-layouts.hog') | Out-Null
    Invoke-RuntimeLayoutAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-runtime-layouts.mn2') | Out-Null
    Invoke-RuntimeLayoutPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-runtime-layouts.hog files/d2x-redux/missions/rtlayout.hog && cp /data/local/tmp/xl-runtime-layouts.mn2 files/d2x-redux/missions/rtlayout.mn2' | Out-Null
    Invoke-RuntimeLayoutAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_runtime_layouts.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-RuntimeLayoutAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-RuntimeLayoutPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-RuntimeLayoutPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-RuntimeLayoutPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "RuntimeLayout runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete RuntimeLayout runtime result' }
    Invoke-RuntimeLayoutAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-runtime-layouts.png') | Out-Null
    Invoke-RuntimeLayoutAdb -Arguments @('pull', '/data/local/tmp/xl-runtime-layouts.png', "$output/render.png") | Out-Null
    Write-Host "PASS: RuntimeLayout Android format/light actions, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-RuntimeLayoutAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-RuntimeLayoutPrivate "rm -rf $path" | Out-Null }
            Invoke-RuntimeLayoutPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-RuntimeLayoutPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "RuntimeLayout test state restoration differs; backup retained at $backup" }
            Invoke-RuntimeLayoutPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-RuntimeLayoutAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-runtime-layouts.hog', '/data/local/tmp/xl-runtime-layouts.mn2', '/data/local/tmp/xl-runtime-layouts.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
