#!/usr/bin/env pwsh
# Extended HOG ownership, disk/rewind and stock resource isolation
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '1b8b75b829a865191b3dd6686d3eababfd9de692464a1a5dcc5b1410e13bc6c0'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'a4a1be94729745d47fb34b5b83beeff85ed4379f3126c30556a2351ea611f589') {
    throw 'Requires the pinned derived hog fixture from xl_extended_hog'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-extended-hog-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-HogAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Hog adb failed: $result" }
    return $result
}
function Invoke-HogPrivate {
    param([string]$Command)
    Invoke-HogAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-extended-hog-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/xhogs.hog', 'files/d2x-redux/missions/xhogs.mn2')
$ready = $false
try {
    Invoke-HogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-HogPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-HogPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-HogPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-HogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-HogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-hog.hog') | Out-Null
    Invoke-HogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-hog.mn2') | Out-Null
    Invoke-HogPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-hog.hog files/d2x-redux/missions/xhogs.hog && cp /data/local/tmp/xl-hog.mn2 files/d2x-redux/missions/xhogs.mn2' | Out-Null
    Invoke-HogAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_hog_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-HogAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-HogPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-HogPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-HogPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Hog runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Hog runtime result' }
    Write-Host "PASS: XL extended archive ownership, exact disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-HogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-HogPrivate "rm -rf $path" | Out-Null }
            Invoke-HogPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-HogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Hog test state restoration differs; backup retained at $backup" }
            Invoke-HogPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-HogAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-hog.hog', '/data/local/tmp/xl-hog.mn2') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
