#!/usr/bin/env pwsh
# Derived XL close_door crossings, blocking, visibility and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '3c8a1f6241a9f06df3931aa088606ad16ec6e7d4df0a333669b74046e4e62e9b'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '6194d572c9269c95fd2d5f2586f65f0d9e3a4516dd790fbe933a330eda148245') {
    throw 'Requires the pinned derived fixture from xl_close_doors'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-close_door-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-CloseDoorAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "CloseDoor adb failed: $result" }
    return $result
}
function Invoke-CloseDoorPrivate {
    param([string]$Command)
    Invoke-CloseDoorAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-close_door-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/close.hog', 'files/d2x-redux/missions/close.mn2')
$ready = $false
try {
    Invoke-CloseDoorAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-CloseDoorPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-CloseDoorPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-CloseDoorPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-CloseDoorPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-CloseDoorAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-close.hog') | Out-Null
    Invoke-CloseDoorAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-close.mn2') | Out-Null
    Invoke-CloseDoorPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-close.hog files/d2x-redux/missions/close.hog && cp /data/local/tmp/xl-close.mn2 files/d2x-redux/missions/close.mn2' | Out-Null
    Invoke-CloseDoorAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_close_door_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-CloseDoorAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-CloseDoorPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-CloseDoorPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-CloseDoorPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "CloseDoor runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete CloseDoor runtime result' }
    Invoke-CloseDoorAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-close_door.png') | Out-Null
    Invoke-CloseDoorAdb -Arguments @('pull', '/data/local/tmp/xl-close_door.png', "$output/render.png") | Out-Null
    Write-Host "PASS: CloseDoor Android animation/crossings/blocking/visibility, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-CloseDoorAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-CloseDoorPrivate "rm -rf $path" | Out-Null }
            Invoke-CloseDoorPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-CloseDoorPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "CloseDoor test state restoration differs; backup retained at $backup" }
            Invoke-CloseDoorPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-CloseDoorAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-close.hog', '/data/local/tmp/xl-close.mn2', '/data/local/tmp/xl-close_door.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
