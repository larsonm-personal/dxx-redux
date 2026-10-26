#!/usr/bin/env pwsh
# Derived wall trigger chains, counter state and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = 'b02d206a3ccfd1b753adb447eb68813e46af6caa6e3c7e7533be994d0b71f60b'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '5b66c41a46b472a8eb95603a21abbe8193fe1d64e050a80c494d0f74d0756575') {
    throw 'Requires the pinned derived fixture from xl_wall_trigger_chains'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-wall-trigger-chains-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-TriggerChainsAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "TriggerChains adb failed: $result" }
    return $result
}
function Invoke-TriggerChainsPrivate {
    param([string]$Command)
    Invoke-TriggerChainsAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-trigger_chains-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/chain.hog', 'files/d2x-redux/missions/chain.mn2')
$ready = $false
try {
    Invoke-TriggerChainsAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-TriggerChainsPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-TriggerChainsPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-TriggerChainsPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-TriggerChainsPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-TriggerChainsAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-trigger_chains.hog') | Out-Null
    Invoke-TriggerChainsAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-trigger_chains.mn2') | Out-Null
    Invoke-TriggerChainsPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-trigger_chains.hog files/d2x-redux/missions/chain.hog && cp /data/local/tmp/xl-trigger_chains.mn2 files/d2x-redux/missions/chain.mn2' | Out-Null
    Invoke-TriggerChainsAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_wall_trigger_chains_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 240
    $testExit = $LASTEXITCODE
    Invoke-TriggerChainsAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-TriggerChainsPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-TriggerChainsPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-TriggerChainsPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "TriggerChains runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete TriggerChains runtime result' }
    Invoke-TriggerChainsAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-trigger_chains.png') | Out-Null
    Invoke-TriggerChainsAdb -Arguments @('pull', '/data/local/tmp/xl-trigger_chains.png', "$output/render.png") | Out-Null
    Write-Host "PASS: TriggerChains Android startup/rendering, save/restore and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-TriggerChainsAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-TriggerChainsPrivate "rm -rf $path" | Out-Null }
            Invoke-TriggerChainsPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-TriggerChainsPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "TriggerChains test state restoration differs; backup retained at $backup" }
            Invoke-TriggerChainsPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-TriggerChainsAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-trigger_chains.hog', '/data/local/tmp/xl-trigger_chains.mn2', '/data/local/tmp/xl-trigger_chains.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
