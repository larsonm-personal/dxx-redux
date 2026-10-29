#!/usr/bin/env pwsh
# Authored fog GPU controls, normal rendering and level transitions; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [switch]$ExpectDepthRejection
)
$ErrorActionPreference = 'Stop'
$expectedHog = '56b224bdec60f8b89a55e51d5b9d9ab548c00cd27a8e75493923253b66c00c1e'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'e4d88a385ce95b952a5bcadb0c470e4500c7cb0fe67872a8e31c81f2ec7ba41b') {
    throw 'Requires the pinned derived fixture from xl_fog_fixture'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$artifactName = if ($ExpectDepthRejection) { 'xl-fog-driver-rejection' } else { 'xl-fog-android' }
$scriptName = if ($ExpectDepthRejection) { 'test_xl_fog_driver_rejection.jsonc' } else { 'test_xl_fog_runtime.jsonc' }
$output = Join-Path $PSScriptRoot "../../temp/$artifactName"
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
# Do not leave a previous run's PASS/state beside a new host or capture failure
foreach ($name in @('result.json', 'state.json', 'steps.jsonl', 'logcat.txt', 'render.png')) {
    Remove-Item -LiteralPath (Join-Path $output $name) -Force -ErrorAction SilentlyContinue
}
function Invoke-FogAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Fog adb failed: $result" }
    return $result
}
function Invoke-FogPrivate {
    param([string]$Command)
    Invoke-FogAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-fog-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/graphics_safety.json', 'files/.graphics_safety.lock',
    'files/d2x-redux/missions/fogctrl.hog', 'files/d2x-redux/missions/fogctrl.mn2')
$ready = $false
try {
    Invoke-FogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-FogPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-FogPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-FogPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-FogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-FogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-fog.hog') | Out-Null
    Invoke-FogAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-fog.mn2') | Out-Null
    Invoke-FogPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-fog.hog files/d2x-redux/missions/fogctrl.hog && cp /data/local/tmp/xl-fog.mn2 files/d2x-redux/missions/fogctrl.mn2' | Out-Null
    Invoke-FogAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $scriptName -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-FogAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-FogPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-FogPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-FogPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Fog runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Fog runtime result' }
    Invoke-FogAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-fog.png') | Out-Null
    Invoke-FogAdb -Arguments @('pull', '/data/local/tmp/xl-fog.png', "$output/render.png") | Out-Null
    Write-Host "PASS: $scriptName ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-FogAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-FogPrivate "rm -rf $path" | Out-Null }
            Invoke-FogPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-FogPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Fog test state restoration differs; backup retained at $backup" }
            Invoke-FogPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-FogAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-fog.hog', '/data/local/tmp/xl-fog.mn2', '/data/local/tmp/xl-fog.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
