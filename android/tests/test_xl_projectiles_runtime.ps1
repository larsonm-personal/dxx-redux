#!/usr/bin/env pwsh
# Controlled production projectile contacts, disk/rewind, reuse and Redux isolation
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'f5d3cc0a2f56acbafafc282883bf0e5078aff60706d8bf0fd03f4c5dc311c753') {
    throw 'Requires the pinned original Neptune HOG and descriptor'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-projectiles-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-ProjectilesAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Projectiles adb failed: $result" }
    return $result
}
function Invoke-ProjectilesPrivate {
    param([string]$Command)
    Invoke-ProjectilesAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-projectiles-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/neptunxl.hog', 'files/d2x-redux/missions/neptunxl.mn2')
$ready = $false
try {
    Invoke-ProjectilesAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-ProjectilesPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-ProjectilesPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-ProjectilesPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-ProjectilesPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-ProjectilesAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-neptunxl.hog') | Out-Null
    Invoke-ProjectilesAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-neptunxl.mn2') | Out-Null
    Invoke-ProjectilesPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-neptunxl.hog files/d2x-redux/missions/neptunxl.hog && cp /data/local/tmp/xl-neptunxl.mn2 files/d2x-redux/missions/neptunxl.mn2' | Out-Null
    Invoke-ProjectilesAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_projectiles_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-ProjectilesAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-ProjectilesPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-ProjectilesPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-ProjectilesPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Projectiles runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Projectiles runtime result' }
    Write-Host "PASS: XL projectile history, damage, disk/rewind, reuse and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-ProjectilesAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-ProjectilesPrivate "rm -rf $path" | Out-Null }
            Invoke-ProjectilesPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-ProjectilesPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Projectiles test state restoration differs; backup retained at $backup" }
            Invoke-ProjectilesPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-ProjectilesAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-neptunxl.hog', '/data/local/tmp/xl-neptunxl.mn2') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
