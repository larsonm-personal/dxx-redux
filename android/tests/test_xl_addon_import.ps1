#!/usr/bin/env pwsh
param([string]$Serial, [ValidatePattern('^[A-Za-z0-9_.-]+\.7z$')][string]$DistributionName = '')
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-addon-import-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Path $output -Force | Out-Null
function Invoke-AddonAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 180 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Addon import adb failed: $result" }
    return $result
}
function Invoke-AddonPrivate {
    param([string]$Command)
    Invoke-AddonAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-addon-import-backup'
$paths = @('shared_prefs', 'files/file_sets.json')
$ready = $false
try {
    Invoke-AddonAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-AddonPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-AddonPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count) { Invoke-AddonPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null }
    $before = if ($existing.Count) { @(Invoke-AddonPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object } else { @() }
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-AddonAdb -Arguments @('logcat', '-c') | Out-Null
    $instrument = @('shell', 'am', 'instrument', '-w', '-e', 'suite', 'xl_addon_import')
    if ($DistributionName) { $instrument += @('-e', 'xl_addon_distribution', $DistributionName) }
    $instrument += "$($script:PACKAGE).test/com.dxxredux.app.RecoveryInstrumentation"
    $result = Invoke-AddonAdb -Arguments $instrument
    $result | Set-Content "$output/instrumentation.txt" -Encoding utf8
    Write-Output $result
    if ($result -notmatch 'PASS: XL addon selective import' -or $result -match 'FAIL:|INSTRUMENTATION_FAILED|Process crashed') {
        throw 'XL addon import checks failed'
    }
    $render = "/sdcard/Android/data/$($script:PACKAGE)/files/xl-addon-picker.png"
    Invoke-AddonAdb -Arguments @('pull', $render, "$output/render.png") | Out-Null
    Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', $render) | Out-Null
    if ($DistributionName) {
        $originalRender = "/sdcard/Android/data/$($script:PACKAGE)/files/xl-addon-picker-original.png"
        Invoke-AddonAdb -Arguments @('pull', $originalRender, "$output/original-render.png") | Out-Null
        Invoke-AddonAdb -Arguments @('shell', 'rm', '-f', $originalRender) | Out-Null
    }
} finally {
    try {
        $captureFailure = $null
        try {
            Invoke-AddonAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
        } catch { $captureFailure = $_ }
        Invoke-AddonAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            foreach ($path in $paths) { Invoke-AddonPrivate "rm -rf $path" | Out-Null }
            if ($existing.Count) { Invoke-AddonPrivate "tar -xf $backup/state.tar" | Out-Null }
            $after = if ($existing.Count) { @(Invoke-AddonPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object } else { @() }
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object @($before) @($after)) { throw "Addon import profile restoration differs; backup retained at $backup" }
            Invoke-AddonPrivate "rm -rf $backup" | Out-Null
            Write-Output 'PASS: Addon UI profile restoration hashes match'
        }
        if ($captureFailure) { throw $captureFailure }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
