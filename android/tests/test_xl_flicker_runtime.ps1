#!/usr/bin/env pwsh
# Full XL flicker phase, authored controls, disk/rewind and Redux isolation
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = 'a33c847e99e9ee4a511b8f272ccdd84f568311de1c8b813dd6019e4114b2c8ac'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'dbf359ac93ecaeef6234f242b9fb9829c078b6035929f377d87091fd1b16d4b3') {
    throw 'Requires the pinned derived flicker fixture from xl_flickers'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-flickers-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-FlickerAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Flicker adb failed: $result" }
    return $result
}
function Invoke-FlickerPrivate {
    param([string]$Command)
    Invoke-FlickerAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-flickers-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/flicker.hog', 'files/d2x-redux/missions/flicker.mn2')
$ready = $false
try {
    Invoke-FlickerAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-FlickerPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-FlickerPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-FlickerPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-FlickerPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-FlickerAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-flicker.hog') | Out-Null
    Invoke-FlickerAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-flicker.mn2') | Out-Null
    Invoke-FlickerPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-flicker.hog files/d2x-redux/missions/flicker.hog && cp /data/local/tmp/xl-flicker.mn2 files/d2x-redux/missions/flicker.mn2' | Out-Null
    Invoke-FlickerAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_flicker_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-FlickerAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-FlickerPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-FlickerPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-FlickerPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Flicker runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Flicker runtime result' }
    Write-Host "PASS: XL full flicker phases, authored restart, exact disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-FlickerAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-FlickerPrivate "rm -rf $path" | Out-Null }
            Invoke-FlickerPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-FlickerPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Flicker test state restoration differs; backup retained at $backup" }
            Invoke-FlickerPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-FlickerAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-flicker.hog', '/data/local/tmp/xl-flicker.mn2') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
