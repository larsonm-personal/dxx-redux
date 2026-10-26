#!/usr/bin/env pwsh
# Derived wall teleport activation and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = 'b3c793340be3cfbc9f1281a8f55d378281da05576044cbaef9870e84cc69756b'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '322b0be65efef22c34ba0f82398dbaef50a0455f2efdb7cb9e691e11c8fff293') {
    throw 'Requires the pinned derived fixture from xl_wall_teleports'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-wall-teleports-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-WallTeleportsAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "WallTeleports adb failed: $result" }
    return $result
}
function Invoke-WallTeleportsPrivate {
    param([string]$Command)
    Invoke-WallTeleportsAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-wall_teleports-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/teleport.hog', 'files/d2x-redux/missions/teleport.mn2')
$ready = $false
try {
    Invoke-WallTeleportsAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-WallTeleportsPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-WallTeleportsPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-WallTeleportsPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-WallTeleportsPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-WallTeleportsAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-wall_teleports.hog') | Out-Null
    Invoke-WallTeleportsAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-wall_teleports.mn2') | Out-Null
    Invoke-WallTeleportsPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-wall_teleports.hog files/d2x-redux/missions/teleport.hog && cp /data/local/tmp/xl-wall_teleports.mn2 files/d2x-redux/missions/teleport.mn2' | Out-Null
    Invoke-WallTeleportsAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_wall_teleports_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 240
    $testExit = $LASTEXITCODE
    Invoke-WallTeleportsAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-WallTeleportsPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-WallTeleportsPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-WallTeleportsPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "WallTeleports runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete WallTeleports runtime result' }
    Invoke-WallTeleportsAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-wall_teleports.png') | Out-Null
    Invoke-WallTeleportsAdb -Arguments @('pull', '/data/local/tmp/xl-wall_teleports.png', "$output/render.png") | Out-Null
    Write-Host "PASS: WallTeleports Android startup/rendering, save/restore and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-WallTeleportsAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-WallTeleportsPrivate "rm -rf $path" | Out-Null }
            Invoke-WallTeleportsPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-WallTeleportsPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "WallTeleports test state restoration differs; backup retained at $backup" }
            Invoke-WallTeleportsPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-WallTeleportsAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-wall_teleports.hog', '/data/local/tmp/xl-wall_teleports.mn2', '/data/local/tmp/xl-wall_teleports.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}

