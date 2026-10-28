#!/usr/bin/env pwsh
# Derived XL cloak crossings, blocking, visibility and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = 'bcd12bae0b13a1e739a3daaa4ab10a21b8623087902aa94da7b66bd7e09c6bcb'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'c5291338fda45ee11516ebffae55826f27e5b64eb51ddac07798e4d6875cb98d') {
    throw 'Requires the pinned derived fixture from xl_cloaks'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-cloak-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-CloakAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Cloak adb failed: $result" }
    return $result
}
function Invoke-CloakPrivate {
    param([string]$Command)
    Invoke-CloakAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-cloak-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/cloak.hog', 'files/d2x-redux/missions/cloak.mn2')
$ready = $false
try {
    Invoke-CloakAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-CloakPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-CloakPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-CloakPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-CloakPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-CloakAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-cloak.hog') | Out-Null
    Invoke-CloakAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-cloak.mn2') | Out-Null
    Invoke-CloakPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-cloak.hog files/d2x-redux/missions/cloak.hog && cp /data/local/tmp/xl-cloak.mn2 files/d2x-redux/missions/cloak.mn2' | Out-Null
    Invoke-CloakAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_cloak_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-CloakAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-CloakPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-CloakPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-CloakPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Cloak runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Cloak runtime result' }
    Invoke-CloakAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-cloak.png') | Out-Null
    Invoke-CloakAdb -Arguments @('pull', '/data/local/tmp/xl-cloak.png', "$output/render.png") | Out-Null
    $state = Get-Content "$output/state.json" -Raw | ConvertFrom-Json
    $expectedAlpha = 1.0 - $state.xl_pilot.cloak_draw_value / 31.0
    if ([Math]::Abs($state.xl_pilot.cloak_draw_alpha - $expectedAlpha) -gt 0.000001) { throw 'Actual cloak draw alpha differs from the pinned XL scale' }
    Write-Host "PASS: Cloak Android animation/crossings/blocking/visibility, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-CloakAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-CloakPrivate "rm -rf $path" | Out-Null }
            Invoke-CloakPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-CloakPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Cloak test state restoration differs; backup retained at $backup" }
            Invoke-CloakPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-CloakAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-cloak.hog', '/data/local/tmp/xl-cloak.mn2', '/data/local/tmp/xl-cloak.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
