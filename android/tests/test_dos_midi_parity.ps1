#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [string]$BuildDirectory = 'android/build/host-extract-tests',
    [string]$ReferenceDirectory = 'game_data/music/dos-references',
    [string]$OutputDirectory = 'temp/midi-parity/reference',
    [ValidateSet('all', '2', '7')][string]$Level = 'all',
    [switch]$SkipBuild,
    [switch]$SyntheticOnly
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../helpers/test_env.ps1"
if (-not $SkipBuild) {
    cmake -S android/app/src/main/cpp/extract -B $BuildDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Host test configure failed' }
    cmake --build $BuildDirectory --config Release --target hmp_midi_export midi_tsf_render test_hmp_android_shared test_midi_seek_timeline
    if ($LASTEXITCODE -ne 0) { throw 'Host MIDI test build failed' }
}
$exporter = Join-Path $BuildDirectory 'Release/hmp_midi_export.exe'
if (-not (Test-Path -LiteralPath $exporter)) { $exporter = Join-Path $BuildDirectory 'hmp_midi_export' }
ctest --test-dir $BuildDirectory -C Release --output-on-failure -R '^(hmp_android_shared_tests|midi_seek_timeline_tests|hmp_playback_tests)$'
if ($LASTEXITCODE -ne 0) { throw 'Native MIDI tests failed' }
if (-not $SyntheticOnly) {
    & "$PSScriptRoot/../helpers/retain-recent-artifacts.ps1" -Artifacts $OutputDirectory
    $levels = if ($Level -eq 'all') { @(2, 7) } else { @([int]$Level) }
    foreach ($number in $levels) {
        $name = 'descent14-game{0:D2}' -f $number
        $reference = Join-Path $ReferenceDirectory $name
        $output = Join-Path $OutputDirectory $name
        python "$PSScriptRoot/run_dos_midi_parity.py" --exporter $exporter --fixture "$PSScriptRoot/fixtures/dos-midi/$name.json" --reference $reference --output $output
        if ($LASTEXITCODE -ne 0) { throw "DOS MIDI Level $number reference parity failed" }
    }
}
