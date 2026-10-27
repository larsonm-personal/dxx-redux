#!/usr/bin/env pwsh
# Derived XL flag cues, saved actions and stock isolation; controlled poses only
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne '6f2adcc1834786068db52703c6ef9d842b5f7cc60797d706937b2b470daa489b' -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'f11d50656e8181b8bf104d306cebc0ab13546cf1ba2ff9bae988aabeb4711bc5') {
    throw 'Requires the pinned derived trigger flag fixture'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-trigger-flags-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-FlagAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Trigger flag adb failed: $result" }
    return $result
}
function Invoke-FlagPrivate {
    param([string]$Command)
    Invoke-FlagAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-trigger-flags-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/trigflag.hog', 'files/d2x-redux/missions/trigflag.mn2')
$ready = $false
try {
    Invoke-FlagAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-FlagPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-FlagPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-FlagPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-FlagPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-FlagAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-trigger-flags.hog') | Out-Null
    Invoke-FlagAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-trigger-flags.mn2') | Out-Null
    Invoke-FlagPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-trigger-flags.hog files/d2x-redux/missions/trigflag.hog && cp /data/local/tmp/xl-trigger-flags.mn2 files/d2x-redux/missions/trigflag.mn2' | Out-Null
    Invoke-FlagAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_trigger_flags_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    $testExit = $LASTEXITCODE
    Invoke-FlagAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-FlagPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-FlagPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-FlagPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Trigger flag runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete trigger flag runtime result' }
    Write-Host "PASS: XL trigger flag mixer, authored actions, disk/rewind and stock isolation ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-FlagAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-FlagPrivate "rm -rf $path" | Out-Null }
            Invoke-FlagPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-FlagPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Trigger flag restoration differs; backup retained at $backup" }
            Invoke-FlagPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-FlagAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-trigger-flags.hog', '/data/local/tmp/xl-trigger-flags.mn2') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
