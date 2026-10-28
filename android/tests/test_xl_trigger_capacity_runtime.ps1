#!/usr/bin/env pwsh
# Authored high-ID wall actions, physical crossing, disk/rewind and stock isolation
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '7ee5cd2029d1f24990d5d882f1dcd35fd084e439352c5c942a14b858c51f0dbf'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'e843e80a0e56225d10a6a71510a6b637039fa6d3915748b1e40d39cae85c85b0') {
    throw 'Requires the pinned derived fixture from xl_trigger_capacity'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-trigger-capacity-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-TriggerCapacityAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "TriggerCapacity adb failed: $result" }
    return $result
}
function Invoke-TriggerCapacityPrivate {
    param([string]$Command)
    Invoke-TriggerCapacityAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-trigger-capacity-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/trigcap.hog', 'files/d2x-redux/missions/trigcap.mn2')
$ready = $false
try {
    Invoke-TriggerCapacityAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-TriggerCapacityPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-TriggerCapacityPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-TriggerCapacityPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-TriggerCapacityPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-TriggerCapacityAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-trigcap.hog') | Out-Null
    Invoke-TriggerCapacityAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-trigcap.mn2') | Out-Null
    Invoke-TriggerCapacityPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-trigcap.hog files/d2x-redux/missions/trigcap.hog && cp /data/local/tmp/xl-trigcap.mn2 files/d2x-redux/missions/trigcap.mn2' | Out-Null
    Invoke-TriggerCapacityAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_trigger_capacity_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-TriggerCapacityAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-TriggerCapacityPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-TriggerCapacityPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-TriggerCapacityPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "TriggerCapacity runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete TriggerCapacity runtime result' }
    Write-Host "PASS: XL trigger identities, physical crossing and scheduling, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-TriggerCapacityAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-TriggerCapacityPrivate "rm -rf $path" | Out-Null }
            Invoke-TriggerCapacityPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-TriggerCapacityPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "TriggerCapacity test state restoration differs; backup retained at $backup" }
            Invoke-TriggerCapacityPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-TriggerCapacityAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-trigcap.hog', '/data/local/tmp/xl-trigcap.mn2') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
