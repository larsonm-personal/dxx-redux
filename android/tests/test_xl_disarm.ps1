#!/usr/bin/env pwsh
# XL disarming, real attack consumers and disk/rewind; no traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '58ca2f69cea79ae6ade83f17019b5ee58e9f198771e88c59011ed2ade94efd43'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'c18be494d3dcc22471d42c37f200a30c4ce6175309dd32fb0b932e3bf060c689') {
    throw 'Requires the pinned derived fixture from xl_disarm'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-disarm-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-DisarmAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Disarm adb failed: $result" }
    return $result
}
function Invoke-DisarmPrivate {
    param([string]$Command)
    Invoke-DisarmAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-disarm-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/xdisarm.hog', 'files/d2x-redux/missions/xdisarm.mn2')
$ready = $false
try {
    Invoke-DisarmAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-DisarmPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-DisarmPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-DisarmPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-DisarmPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-DisarmAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-disarm.hog') | Out-Null
    Invoke-DisarmAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-disarm.mn2') | Out-Null
    Invoke-DisarmPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-disarm.hog files/d2x-redux/missions/xdisarm.hog && cp /data/local/tmp/xl-disarm.mn2 files/d2x-redux/missions/xdisarm.mn2' | Out-Null
    Invoke-DisarmAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_disarm.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-DisarmAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-DisarmPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-DisarmPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-DisarmPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Disarm runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Disarm runtime result' }
    Invoke-DisarmAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-disarm.png') | Out-Null
    Invoke-DisarmAdb -Arguments @('pull', '/data/local/tmp/xl-disarm.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Disarm Android attack consumers, robot lifetimes and actions, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-DisarmAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-DisarmPrivate "rm -rf $path" | Out-Null }
            Invoke-DisarmPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-DisarmPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Disarm test state restoration differs; backup retained at $backup" }
            Invoke-DisarmPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-DisarmAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-disarm.hog', '/data/local/tmp/xl-disarm.mn2', '/data/local/tmp/xl-disarm.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
