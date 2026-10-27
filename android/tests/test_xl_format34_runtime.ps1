#!/usr/bin/env pwsh
# Derived XL format34 light actions and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = 'c7d8f979382f162fc17d4c13976b234ba2dc5f9dbd55b5bde6314230f229a378'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '8cc94a3d53ce1e251a26d9fa4b2c17e18b595b2dbe60dc1896503271463af5b5') {
    throw 'Requires the pinned derived fixture from xl_format34_native'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-format34-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-Format34Adb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Format34 adb failed: $result" }
    return $result
}
function Invoke-Format34Private {
    param([string]$Command)
    Invoke-Format34Adb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-format34-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/format34.hog', 'files/d2x-redux/missions/format34.mn2')
$ready = $false
try {
    Invoke-Format34Adb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-Format34Private "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-Format34Private "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-Format34Private "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-Format34Private "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-Format34Adb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-format34.hog') | Out-Null
    Invoke-Format34Adb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-format34.mn2') | Out-Null
    Invoke-Format34Private 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-format34.hog files/d2x-redux/missions/format34.hog && cp /data/local/tmp/xl-format34.mn2 files/d2x-redux/missions/format34.mn2' | Out-Null
    Invoke-Format34Adb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_format34_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-Format34Adb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-Format34Private 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-Format34Private 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-Format34Private 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Format34 runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Format34 runtime result' }
    Invoke-Format34Adb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-format34.png') | Out-Null
    Invoke-Format34Adb -Arguments @('pull', '/data/local/tmp/xl-format34.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Format34 Android format/light actions, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-Format34Adb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-Format34Private "rm -rf $path" | Out-Null }
            Invoke-Format34Private "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-Format34Private "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Format34 test state restoration differs; backup retained at $backup" }
            Invoke-Format34Private "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-Format34Adb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-format34.hog', '/data/local/tmp/xl-format34.mn2', '/data/local/tmp/xl-format34.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
