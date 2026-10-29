#!/usr/bin/env pwsh
# Derived XL keyed-wall crossings, blocking, visibility and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '3408935bb892773d6b0bc7d295db923919e001d916eff5aef7e0ec1bbeddd8fe'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'e3c111d43ae21e63ae7d3e23e33bbbbe78b4976fb44e89831c50775c23db3577') {
    throw 'Requires the pinned derived fixture from xl_keyed_closed_walls'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-keyed_wall-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-KeyedWallAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Keyed-wall adb failed: $result" }
    return $result
}
function Invoke-KeyedWallPrivate {
    param([string]$Command)
    Invoke-KeyedWallAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-keyed_wall-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/keywall.hog', 'files/d2x-redux/missions/keywall.mn2')
$ready = $false
try {
    Invoke-KeyedWallAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-KeyedWallPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-KeyedWallPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-KeyedWallPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-KeyedWallPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-KeyedWallAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-keywall.hog') | Out-Null
    Invoke-KeyedWallAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-keywall.mn2') | Out-Null
    Invoke-KeyedWallPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-keywall.hog files/d2x-redux/missions/keywall.hog && cp /data/local/tmp/xl-keywall.mn2 files/d2x-redux/missions/keywall.mn2' | Out-Null
    Invoke-KeyedWallAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_keyed_wall_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-KeyedWallAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-KeyedWallPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-KeyedWallPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-KeyedWallPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Keyed-wall runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Keyed-wall runtime result' }
    Invoke-KeyedWallAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-keyed_wall.png') | Out-Null
    Invoke-KeyedWallAdb -Arguments @('pull', '/data/local/tmp/xl-keyed_wall.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Keyed-wall Android keys/crossings/blocking/visibility, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-KeyedWallAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-KeyedWallPrivate "rm -rf $path" | Out-Null }
            Invoke-KeyedWallPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-KeyedWallPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Keyed-wall test state restoration differs; backup retained at $backup" }
            Invoke-KeyedWallPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-KeyedWallAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-keywall.hog', '/data/local/tmp/xl-keywall.mn2', '/data/local/tmp/xl-keyed_wall.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
