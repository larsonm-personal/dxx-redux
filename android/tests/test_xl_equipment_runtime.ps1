#!/usr/bin/env pwsh
# Derived XL equipment production and disk/rewind persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor
)
$ErrorActionPreference = 'Stop'
$expectedHog = 'ebf2d9080dde9740cf18dd9ee2da3a48ddde08c11cfdd3dc21214af8c9dd4e8b'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne '6a4ca565352f3fe1bdce3b3249672c5ad2a2844816ac87173f21d8110960f6d0') {
    throw 'Requires the pinned derived fixture from xl_equipment_generators'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-equipment-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-EquipmentAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Equipment adb failed: $result" }
    return $result
}
function Invoke-EquipmentPrivate {
    param([string]$Command)
    Invoke-EquipmentAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-equipment-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/equip.hog', 'files/d2x-redux/missions/equip.mn2')
$ready = $false
try {
    Invoke-EquipmentAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-EquipmentPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-EquipmentPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-EquipmentPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-EquipmentPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-EquipmentAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-equipment.hog') | Out-Null
    Invoke-EquipmentAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-equipment.mn2') | Out-Null
    Invoke-EquipmentPrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-equipment.hog files/d2x-redux/missions/equip.hog && cp /data/local/tmp/xl-equipment.mn2 files/d2x-redux/missions/equip.mn2' | Out-Null
    Invoke-EquipmentAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName 'test_xl_equipment_runtime.jsonc' -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 360
    $testExit = $LASTEXITCODE
    Invoke-EquipmentAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-EquipmentPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-EquipmentPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-EquipmentPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Equipment runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Equipment runtime result' }
    Invoke-EquipmentAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-equipment.png') | Out-Null
    Invoke-EquipmentAdb -Arguments @('pull', '/data/local/tmp/xl-equipment.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Equipment Android activation/production/pickup, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-EquipmentAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-EquipmentPrivate "rm -rf $path" | Out-Null }
            Invoke-EquipmentPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-EquipmentPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Equipment test state restoration differs; backup retained at $backup" }
            Invoke-EquipmentPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-EquipmentAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-equipment.hog', '/data/local/tmp/xl-equipment.mn2', '/data/local/tmp/xl-equipment.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
