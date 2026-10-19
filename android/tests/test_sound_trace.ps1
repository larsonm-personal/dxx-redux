#!/usr/bin/env pwsh
# Stock playback trace smoke test on the selected Android test target
param([switch]$Install)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
Initialize-AndroidTestTarget | Out-Null
Assert-IsolatedPhysicalTestApp
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$logPath = Join-Path $repoRoot 'temp/sound_trace_logcat.txt'
New-Item -ItemType Directory -Force (Split-Path $logPath) | Out-Null
$logSession = $null
try {
    $logSession = Start-AndroidTestDebugLogSession
    & $ADB logcat -c
    if ($LASTEXITCODE -ne 0) { throw 'Could not clear emulator logcat' }
    & (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_sound_trace.jsonc -Game d2 -Install:$Install
    if ($LASTEXITCODE -ne 0) { throw 'Sound trace gameplay automation failed' }
    $records = @(& $ADB logcat -d -s 'DXX-DLOG:D' '*:S')
    if ($LASTEXITCODE -ne 0) { throw 'Could not collect sound trace logcat' }
    $trace = $records -join "`n"
    [IO.File]::WriteAllText($logPath, $trace, [Text.UTF8Encoding]::new($false))
    foreach ($pattern in @(
            '\[SFX\] bank=\d+ file=''descent2\.s22'' loaded_from=',
            '\[SFX\] trace_v=1 context=[1-9]\d* mission=''d2'' level=1',
            '\[SFX\] context=[1-9]\d* robot=37 see=\d+:\d+',
            '\[SFX\] context=[1-9]\d* sample=.*bank_match=1 cache_input_match=1 cache_output_match=1'
        )) {
        if ($trace -notmatch $pattern) { throw "Missing sound trace evidence: $pattern (see $logPath)" }
    }
    # A full-level sound must not get a second copy of the effects slider through
    # distance attenuation, and quieter sounds must retain their own attenuation
    $gainRows = [regex]::Matches($trace, 'channel_volume=(\d+) sound_volume=(\d+) distance=(\d+)')
    $fullLevelSounds = 0
    $quietSounds = 0
    foreach ($row in $gainRows) {
        $channelVolume = [int]$row.Groups[1].Value
        $soundVolume = [int]$row.Groups[2].Value
        $distance = [int]$row.Groups[3].Value
        $expectedDistance = 255 - [Math]::Min(255, [Math]::Floor($soundVolume / 256))
        if ($distance -ne $expectedDistance) {
            throw "Effects startup applies extra attenuation: $($row.Value), expected distance=$expectedDistance"
        }
        if ($channelVolume -eq 20) {
            if ($soundVolume -eq 65536) { $fullLevelSounds++ }
            elseif ($soundVolume -gt 0 -and $soundVolume -lt 65536) { $quietSounds++ }
        }
    }
    if ($fullLevelSounds -eq 0 -or $quietSounds -eq 0) {
        throw "Missing calibrated effects gain coverage: full=$fullLevelSounds quiet=$quietSounds (see $logPath)"
    }
    Write-Host "Sound trace and effects gain checks passed: $logPath"
} finally {
    Stop-AndroidTestDebugLogSession -Session $logSession
}
