#!/usr/bin/env pwsh
# Derived XL repair healing, cues, boundaries and disk/rewind persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [switch]$Legacy17
)
$ErrorActionPreference = 'Stop'
$fixture = if ($Legacy17) { 'oldrep' } else { 'repair' }
$testScript = if ($Legacy17) { 'test_xl_legacy_repair_runtime.jsonc' } else { 'test_xl_repair_runtime.jsonc' }
$expectedDescriptor = 'e66dadc31bb929f865299cb914740c23641ce77ae120bf64c04bccf881010a8a'
$expectedHog = 'cd1ef589c362e9449004e58c740124404c27267ab84a90b17b9397ad1120b031'
if ($Legacy17) {
    $expectedHog = 'aa9dc28e1497d702325a6cdbfe967d50e70d3996d4ba8ff688241361a8d72de2'
    $expectedDescriptor = 'acc8e6e01e0d26049ebcda29407be8fa76bfa78c312347df4cbe660a016aec96'
}
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne $expectedDescriptor) {
    throw 'Requires the pinned derived fixture from xl_repair_centers'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot "../../temp/xl-$fixture-android"
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-RepairAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Repair adb failed: $result" }
    return $result
}
function Invoke-RepairPrivate {
    param([string]$Command)
    Invoke-RepairAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-repair-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    "files/d2x-redux/missions/$fixture.hog", "files/d2x-redux/missions/$fixture.mn2")
$ready = $false
try {
    Invoke-RepairAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-RepairPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-RepairPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-RepairPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-RepairPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-RepairAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-repair.hog') | Out-Null
    Invoke-RepairAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-repair.mn2') | Out-Null
    Invoke-RepairPrivate "mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-repair.hog files/d2x-redux/missions/$fixture.hog && cp /data/local/tmp/xl-repair.mn2 files/d2x-redux/missions/$fixture.mn2" | Out-Null
    Invoke-RepairAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $testScript -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-RepairAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-RepairPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-RepairPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-RepairPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Repair runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Repair runtime result' }
    Invoke-RepairAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-repair.png') | Out-Null
    Invoke-RepairAdb -Arguments @('pull', '/data/local/tmp/xl-repair.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Repair Android healing/cap/audio/automap/boundaries, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-RepairAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-RepairPrivate "rm -rf $path" | Out-Null }
            Invoke-RepairPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-RepairPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Repair test state restoration differs; backup retained at $backup" }
            Invoke-RepairPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-RepairAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-repair.hog', '/data/local/tmp/xl-repair.mn2', '/data/local/tmp/xl-repair.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
