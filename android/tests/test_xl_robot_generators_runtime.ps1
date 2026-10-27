#!/usr/bin/env pwsh
# Derived robot generation and pending/morph state and disk/rewind persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = '1cdbbe99c38ebca641bdd5184d342de21172163f051c3379592b3468f8ff3e77'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '9dc10214bc91ff022d4eac6cc61b1f101d3b788d9bf31a8c05eb4c99ec6e5fb4') {
    throw 'Requires the pinned derived fixture from xl_generator_assignment_runtime'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-robot-generators-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-RobotGeneratorsAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "RobotGenerators adb failed: $result" }
    return $result
}
function Invoke-RobotGeneratorsPrivate {
    param([string]$Command)
    Invoke-RobotGeneratorsAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-robot_generators-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/genmap.hog', 'files/d2x-redux/missions/genmap.mn2')
$ready = $false
try {
    Invoke-RobotGeneratorsAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-RobotGeneratorsPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-RobotGeneratorsPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-RobotGeneratorsPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-RobotGeneratorsPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-RobotGeneratorsAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-robot_generators.hog') | Out-Null
    Invoke-RobotGeneratorsAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-robot_generators.mn2') | Out-Null
    Invoke-RobotGeneratorsPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-robot_generators.hog files/d2x-redux/missions/genmap.hog && cp /data/local/tmp/xl-robot_generators.mn2 files/d2x-redux/missions/genmap.mn2' | Out-Null
    Invoke-RobotGeneratorsAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_robot_generators_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    $testExit = $LASTEXITCODE
    Invoke-RobotGeneratorsAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-RobotGeneratorsPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-RobotGeneratorsPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-RobotGeneratorsPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "RobotGenerators runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete RobotGenerators runtime result' }
    Invoke-RobotGeneratorsAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-robot_generators.png') | Out-Null
    Invoke-RobotGeneratorsAdb -Arguments @('pull', '/data/local/tmp/xl-robot_generators.png', "$output/render.png") | Out-Null
    Write-Host "PASS: RobotGenerators Android activation/spawning/combat, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-RobotGeneratorsAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-RobotGeneratorsPrivate "rm -rf $path" | Out-Null }
            Invoke-RobotGeneratorsPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-RobotGeneratorsPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "RobotGenerators test state restoration differs; backup retained at $backup" }
            Invoke-RobotGeneratorsPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-RobotGeneratorsAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-robot_generators.hog', '/data/local/tmp/xl-robot_generators.mn2', '/data/local/tmp/xl-robot_generators.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
