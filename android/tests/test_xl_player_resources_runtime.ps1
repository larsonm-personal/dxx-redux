#!/usr/bin/env pwsh
# Derived XL player resources, saved actions and stock isolation; controlled poses only
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne '683ec40c043d113f5c5983482f3f22f37bfc2d3cd9893d3e34a7f5fe66be856e' -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '96a8e2da8a41be5a24c989974fc8bb72b464b616929d4b41a64e651c322ea17c') {
    throw 'Requires the pinned derived player resource fixture'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-player-resources-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-ResourceAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Player resource adb failed: $result" }
    return $result
}
function Invoke-ResourcePrivate {
    param([string]$Command)
    Invoke-ResourceAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-player-resources-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/resource.hog', 'files/d2x-redux/missions/resource.mn2')
$ready = $false
try {
    Invoke-ResourceAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-ResourcePrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-ResourcePrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-ResourcePrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-ResourcePrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-ResourceAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-player-resources.hog') | Out-Null
    Invoke-ResourceAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-player-resources.mn2') | Out-Null
    Invoke-ResourcePrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-player-resources.hog files/d2x-redux/missions/resource.hog && cp /data/local/tmp/xl-player-resources.mn2 files/d2x-redux/missions/resource.mn2' | Out-Null
    Invoke-ResourceAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_player_resources_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    $testExit = $LASTEXITCODE
    Invoke-ResourceAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-ResourcePrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-ResourcePrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-ResourcePrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Player resource runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete player resource runtime result' }
    Write-Host "PASS: XL player resources, authored actions, disk/rewind and stock isolation ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-ResourceAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-ResourcePrivate "rm -rf $path" | Out-Null }
            Invoke-ResourcePrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-ResourcePrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Player resource restoration differs; backup retained at $backup" }
            Invoke-ResourcePrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-ResourceAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-player-resources.hog', '/data/local/tmp/xl-player-resources.mn2') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
