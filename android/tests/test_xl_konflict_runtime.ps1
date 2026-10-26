#!/usr/bin/env pwsh
# Original Konflict startup, rendering and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne '914231998b80f3900508fa6847adb8d60b6b78670cf7afa1e20e90c9170e46e1' -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '26865fb664d93eaa54acaec9d857bff15b192e1dfcca1598229388f6f77e3d94') {
    throw 'Requires the unchanged original Konflict HOG and descriptor'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-konflict-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-KonflictAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Konflict adb failed: $result" }
    return $result
}
function Invoke-KonflictPrivate {
    param([string]$Command)
    Invoke-KonflictAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-konflict-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/konflict.hog', 'files/d2x-redux/missions/konflict.mn2')
$ready = $false
try {
    Invoke-KonflictAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-KonflictPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-KonflictPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-KonflictPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-KonflictPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-KonflictAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-konflict.hog') | Out-Null
    Invoke-KonflictAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-konflict.mn2') | Out-Null
    Invoke-KonflictPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-konflict.hog files/d2x-redux/missions/konflict.hog && cp /data/local/tmp/xl-konflict.mn2 files/d2x-redux/missions/konflict.mn2' | Out-Null
    Invoke-KonflictAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_xl_konflict_runtime.jsonc -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 240
    $testExit = $LASTEXITCODE
    Invoke-KonflictAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-KonflictPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-KonflictPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-KonflictPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Konflict runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Konflict runtime result' }
    Invoke-KonflictAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-konflict.png') | Out-Null
    Invoke-KonflictAdb -Arguments @('pull', '/data/local/tmp/xl-konflict.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Konflict Android startup/rendering, save/restore and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-KonflictAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-KonflictPrivate "rm -rf $path" | Out-Null }
            Invoke-KonflictPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-KonflictPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Konflict test state restoration differs; backup retained at $backup" }
            Invoke-KonflictPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-KonflictAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-konflict.hog', '/data/local/tmp/xl-konflict.mn2', '/data/local/tmp/xl-konflict.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
