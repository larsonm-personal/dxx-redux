#!/usr/bin/env pwsh
# Owned XL messages, real display/modal handling and disk/rewind; no traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '54b1d6a89c3d43df371639c9c811790e10a46328e59fa4313ac24818a51c8a80'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '2ee0ea78bdfc3165daacbf1986791079656e327877b8171a3b97965bc765b157') {
    throw 'Requires the pinned derived fixture from xl_level_messages'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-level-messages-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-MessageAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Message adb failed: $result" }
    return $result
}
function Invoke-MessagePrivate {
    param([string]$Command)
    Invoke-MessageAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-level-messages-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/xmessage.hog', 'files/d2x-redux/missions/xmessage.mn2')
$ready = $false
try {
    Invoke-MessageAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-MessagePrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-MessagePrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-MessagePrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-MessagePrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-MessageAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-level-messages.hog') | Out-Null
    Invoke-MessageAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-level-messages.mn2') | Out-Null
    Invoke-MessagePrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-level-messages.hog files/d2x-redux/missions/xmessage.hog && cp /data/local/tmp/xl-level-messages.mn2 files/d2x-redux/missions/xmessage.mn2' | Out-Null
    Invoke-MessageAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_level_messages.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-MessageAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-MessagePrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-MessagePrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-MessagePrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Message runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Message runtime result' }
    Invoke-MessageAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-level-messages.png') | Out-Null
    Invoke-MessageAdb -Arguments @('pull', '/data/local/tmp/xl-level-messages.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Message Android display/modal/actions, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-MessageAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-MessagePrivate "rm -rf $path" | Out-Null }
            Invoke-MessagePrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-MessagePrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Message test state restoration differs; backup retained at $backup" }
            Invoke-MessagePrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-MessageAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-level-messages.hog', '/data/local/tmp/xl-level-messages.mn2', '/data/local/tmp/xl-level-messages.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
