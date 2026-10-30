#!/usr/bin/env pwsh
# Owned XL named audio, captured PCM, pause and disk/rewind; no traversal
param(
    [Parameter(Mandatory)][string]$Serial,
    [Parameter(Mandatory)][string]$Hog,
    [Parameter(Mandatory)][string]$Descriptor,
    [switch]$Seismic,
    [switch]$NamedOnly
)
$ErrorActionPreference = 'Stop'
if ($Seismic -and $NamedOnly) { throw 'Choose only one focused audio script' }
if (-not $Seismic -and -not $NamedOnly) {
    # The normal owner runs both scripts; focused switches retain cheap reruns
    $seismicHog = Join-Path (Split-Path -Parent $Hog) 'xseismic.hog'
    $seismicDescriptor = Join-Path (Split-Path -Parent $Descriptor) 'xseismic.mn2'
    if (-not (Test-Path -LiteralPath $seismicHog) -or -not (Test-Path -LiteralPath $seismicDescriptor)) {
        throw 'Full named-audio acceptance requires sibling xseismic.hog/mn2 from xl_named_audio; use -NamedOnly for a focused regression'
    }
    & $PSCommandPath -Serial $Serial -Hog $Hog -Descriptor $Descriptor -NamedOnly
    & $PSCommandPath -Serial $Serial -Hog $seismicHog -Descriptor $seismicDescriptor -Seismic
    return
}
$expectedHog = if ($Seismic) { 'a5a1dce94983b482c9d3423b0f70f2084ca42f10bc131d8ac19f7805141bb2b8' } else { '87faf44175cf565e857baee2edfafd10c7feed8b57cb4cda0258b85e3e9d44f9' }
$expectedDescriptor = if ($Seismic) { '518740b0f17389cb33c8e66309b8252159249c41b85b628174e9160cc6f6210d' } else { 'aae73cc30cdd3d61b2ce81a8a606e17d6a4b49a2ee1e58b09f8dfb985689d465' }
$mission = if ($Seismic) { 'xseismic' } else { 'xsound' }
$scriptName = if ($Seismic) { 'test_xl_seismic.jsonc' } else { 'test_xl_named_audio.jsonc' }
if ((Get-FileHash -LiteralPath $Hog -Algorithm SHA256).Hash -ne $expectedHog -or
    (Get-FileHash -LiteralPath $Descriptor -Algorithm SHA256).Hash -ne $expectedDescriptor) {
    throw 'Requires the pinned derived fixture from xl_named_audio'
}
. "$PSScriptRoot/../helpers/test_helpers.ps1"
$previousSerial = $env:ANDROID_SERIAL
$Serial = Initialize-AndroidTestTarget -Serial $Serial
Assert-IsolatedPhysicalTestApp
$output = Join-Path $PSScriptRoot "../../temp/xl-named-audio-android/$mission"
& "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $output
New-Item -ItemType Directory -Force -Path $output | Out-Null
function Invoke-NamedAudioAdb {
    param([string[]]$Arguments)
    $diagnostic = $null
    $result = Adb-Timeout -AdbArgs $Arguments -Seconds 60 -IncludeStandardError -DiagnosticResult ([ref]$diagnostic)
    if ($diagnostic.timed_out -or $diagnostic.exit_code -ne 0) { throw "NamedAudio adb failed: $result" }
    return $result
}
function Invoke-NamedAudioPrivate {
    param([string]$Command)
    Invoke-NamedAudioAdb -Arguments @('shell', 'run-as', $script:PACKAGE, 'sh', '-c', "'$Command'")
}
$backup = 'files/xl-named-audio-runtime-backup'
$paths = @('files/d2x-redux/Players', 'files/d2x-redux/descent.cfg', 'files/d2x-redux/d2x.ini',
    'files/d2x-redux/mod_usage.json', 'files/descent.cfg', 'files/file_sets.json', 'shared_prefs',
    "files/d2x-redux/missions/$mission.hog", "files/d2x-redux/missions/$mission.mn2")
$ready = $false
try {
    Invoke-NamedAudioAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
    Invoke-NamedAudioPrivate "mkdir $backup" | Out-Null
    $existing = @($paths | Where-Object { ([string](Invoke-NamedAudioPrivate "if test -e $_; then echo yes; fi")).Trim() -eq 'yes' })
    if ($existing.Count -lt 3) { throw 'Provision retail D2 and player player first' }
    Invoke-NamedAudioPrivate "tar -cf $backup/state.tar $($existing -join ' ')" | Out-Null
    $before = @(Invoke-NamedAudioPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
    $before | Set-Content "$output/before.sha256" -Encoding utf8
    $ready = $true
    Invoke-NamedAudioAdb -Arguments @('push', (Resolve-Path -LiteralPath $Hog).Path, '/data/local/tmp/xl-named-audio.hog') | Out-Null
    Invoke-NamedAudioAdb -Arguments @('push', (Resolve-Path -LiteralPath $Descriptor).Path, '/data/local/tmp/xl-named-audio.mn2') | Out-Null
    Invoke-NamedAudioPrivate "mkdir -p files/d2x-redux/missions && cp /data/local/tmp/xl-named-audio.hog files/d2x-redux/missions/$mission.hog && cp /data/local/tmp/xl-named-audio.mn2 files/d2x-redux/missions/$mission.mn2" | Out-Null
    Invoke-NamedAudioAdb -Arguments @('logcat', '-c') | Out-Null
    & "$PSScriptRoot/../helpers/run_test.ps1" -ScriptName $scriptName -Game d2 -Serial $Serial -PreserveState -LeaveRunning -TimeoutSeconds 480
    $testExit = $LASTEXITCODE
    Invoke-NamedAudioAdb -Arguments @('logcat', '-d', '-v', 'threadtime') | Set-Content "$output/logcat.txt" -Encoding utf8
    Invoke-NamedAudioPrivate 'cat files/automation_result.json' | Set-Content "$output/result.json" -Encoding utf8
    Invoke-NamedAudioPrivate 'cat files/automation_log.jsonl' | Set-Content "$output/steps.jsonl" -Encoding utf8
    Invoke-NamedAudioPrivate 'cat files/introspect.json' | Set-Content "$output/state.json" -Encoding utf8
    if ($testExit -ne 0) { throw "NamedAudio runtime failed ($testExit); see $output" }
    $result = Get-Content "$output/result.json" -Raw | ConvertFrom-Json
    if ($result.result -ne 'PASS' -or $result.steps_completed -ne $result.total_steps) { throw 'Incomplete NamedAudio runtime result' }
    Write-Host "PASS: $mission Android PCM/dispatch/pause, disk/rewind and stock transition ($($result.total_steps) steps)"
} finally {
    try {
        Invoke-NamedAudioAdb -Arguments @('shell', 'am', 'force-stop', $script:PACKAGE) | Out-Null
        if ($ready) {
            # Fixed app-private test paths; preserve unrelated missions and caches
            foreach ($path in $paths) { Invoke-NamedAudioPrivate "rm -rf $path" | Out-Null }
            Invoke-NamedAudioPrivate "tar -xf $backup/state.tar" | Out-Null
            $after = @(Invoke-NamedAudioPrivate "find $($existing -join ' ') -type f -exec sha256sum {} \;") | Sort-Object
            $after | Set-Content "$output/after.sha256" -Encoding utf8
            if (Compare-Object $before $after) { throw "NamedAudio test state restoration differs; backup retained at $backup" }
            Invoke-NamedAudioPrivate "rm $backup/state.tar && rmdir $backup" | Out-Null
            Invoke-NamedAudioAdb -Arguments @('shell', 'rm', '-f', '/data/local/tmp/xl-named-audio.hog', '/data/local/tmp/xl-named-audio.mn2', '/data/local/tmp/xl-named-audio.png') | Out-Null
        }
    } finally {
        if ($previousSerial) { $env:ANDROID_SERIAL = $previousSerial } else { Remove-Item Env:\ANDROID_SERIAL -ErrorAction SilentlyContinue }
    }
}
