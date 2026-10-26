#!/usr/bin/env pwsh
# Original Neptune startup, rendering and persistence; no automated traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [Parameter(Mandatory)][string]$FixtureTool
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne '4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303' -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne 'f5d3cc0a2f56acbafafc282883bf0e5078aff60706d8bf0fd03f4c5dc311c753') {
    throw 'Requires the unchanged original Neptune HOG and descriptor'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot '../../temp/xl-neptune-android'
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
$negativeDirectory = Join-Path $output ('rejections-' + [guid]::NewGuid().ToString('N'))
& $FixtureTool $Hog $negativeDirectory
if ($LASTEXITCODE -ne 0) { throw 'Cannot generate private Neptune rejection controls' }
function Invoke-NeptuneAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "Neptune adb failed: $result" }
    return $result
}
function Invoke-NeptunePrivate {
    param([string]$Command)
    Invoke-NeptuneAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-neptune-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    'files/d2x-redux/missions/neptunxl.hog', 'files/d2x-redux/missions/neptunxl.mn2')
foreach ($variant in 0..2) {
    $paths += "files/d2x-redux/missions/npbad$variant.hog", "files/d2x-redux/missions/npbad$variant.mn2"
}
$ready = $false
try {
    Invoke-NeptuneAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-NeptunePrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-NeptunePrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-NeptunePrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-NeptunePrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-NeptuneAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-neptune.hog') | Out-Null
    Invoke-NeptuneAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-neptune.mn2') | Out-Null
    Invoke-NeptunePrivate 'mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-neptune.hog files/d2x-redux/missions/neptunxl.hog && cp /data/local/tmp/xl-neptune.mn2 files/d2x-redux/missions/neptunxl.mn2' | Out-Null
    foreach ($variant in 0..2) {
        foreach ($extension in @('hog', 'mn2')) {
            $name = "npbad$variant.$extension"
            Invoke-NeptuneAdb -Arguments @('push', (Join-Path $negativeDirectory $name), "/data/local/tmp/$name") | Out-Null
            Invoke-NeptunePrivate "cp /data/local/tmp/$name files/d2x-redux/missions/$name" | Out-Null
            Invoke-NeptuneAdb -Arguments @('shell', 'rm', '-f', "/data/local/tmp/$name") | Out-Null
        }
    }
    Invoke-NeptuneAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName test_xl_neptune_runtime.jsonc -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 240
    $testExit = $LASTEXITCODE
    Invoke-NeptuneAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-NeptunePrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-NeptunePrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-NeptunePrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "Neptune runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete Neptune runtime result' }
    Invoke-NeptuneAdb -Arguments @('shell', 'screencap', '-p', '/data/local/tmp/xl-neptune.png') | Out-Null
    Invoke-NeptuneAdb -Arguments @('pull', '/data/local/tmp/xl-neptune.png', "$output/render.png") | Out-Null
    Write-Host "PASS: Neptune Android startup/rendering, save/restore and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-NeptuneAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-NeptunePrivate "rm -rf $path" | Out-Null }
            Invoke-NeptunePrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-NeptunePrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "Neptune test state restoration differs; backup retained at $backup" }
            Invoke-NeptunePrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-NeptuneAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-neptune.hog', '/data/local/tmp/xl-neptune.mn2', '/data/local/tmp/xl-neptune.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
