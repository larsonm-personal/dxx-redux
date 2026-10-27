#!/usr/bin/env pwsh
# Derived XL delayed object chains, saved actors and stock isolation; controlled poses only
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne 'fb223c50cf0a39402243950bfcfb036db2393ff93c2a0f0b4ab3aaedc0d16d61' -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'fb3dbe5cf205c97b089a69c898ab76a5e478776a6d7d4d811bab88b8de056e25') {
    throw 'Requires the pinned derived delayed object event fixture'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-object-chain-delays-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-DelayAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Delayed object event adb failed: $result" }
    return $result
}
function Invoke-DelayPrivate {
    param([string]$Command)
    Invoke-DelayAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-object-chain-delays-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/objdelay.hog', 'files/d2x-redux/missions/objdelay.mn2')
$ready = $false
try {
    Invoke-DelayAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-DelayPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-DelayPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-DelayPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-DelayPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-DelayAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-object-chain-delays.hog') | Out-Null
    Invoke-DelayAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-object-chain-delays.mn2') | Out-Null
    Invoke-DelayPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-object-chain-delays.hog files/d2x-redux/missions/objdelay.hog && cp /data/local/tmp/xl-object-chain-delays.mn2 files/d2x-redux/missions/objdelay.mn2' | Out-Null
    Invoke-DelayAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_object_chain_delays_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    $testExit = $LASTEXITCODE
    Invoke-DelayAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-DelayPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-DelayPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-DelayPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Delayed object event runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete delayed object event runtime result' }
    Write-Host "PASS: XL delayed object event mixer, authored actions, disk/rewind and stock isolation ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-DelayAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-DelayPrivate "rm -rf $path" | Out-Null }
            Invoke-DelayPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-DelayPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Delayed object event restoration differs; backup retained at $backup" }
            Invoke-DelayPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-DelayAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-object-chain-delays.hog', '/data/local/tmp/xl-object-chain-delays.mn2') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
