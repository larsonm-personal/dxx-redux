#!/usr/bin/env pwsh
# Stock playback trace smoke test; intended for the repository test emulator
param([switch]$Install)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_helpers.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$logPath = Join-Path $repoRoot 'temp/sound_trace_logcat.txt'
New-Item -ItemType Directory -Force (Split-Path $logPath) | Out-Null
& $ADB logcat -c
if ($LASTEXITCODE -ne 0) { throw 'Could not clear emulator logcat' }
& (Join-Path $PSScriptRoot '../helpers/run_test.ps1') -ScriptName test_sound_trace.jsonc -Game d2 -Install:$Install
if ($LASTEXITCODE -ne 0) { throw 'Sound trace gameplay automation failed' }
& $ADB logcat -d -s 'DXX-DLOG:D' '*:S' | Set-Content -LiteralPath $logPath -Encoding utf8
if ($LASTEXITCODE -ne 0) { throw 'Could not collect sound trace logcat' }
$trace = Get-Content -LiteralPath $logPath -Raw
foreach ($pattern in @(
        '\[SFX\] bank=\d+ file=''descent2\.s22'' loaded_from=',
        '\[SFX\] trace_v=1 context=[1-9]\d* mission=''d2'' level=1',
        '\[SFX\] context=[1-9]\d* robot=37 see=\d+:\d+',
        '\[SFX\] context=[1-9]\d* sample=.*bank_match=1 cache_input_match=1 cache_output_match=1'
    )) {
    if ($trace -notmatch $pattern) { throw "Missing sound trace evidence: $pattern (see $logPath)" }
}
Write-Host "Sound trace smoke test passed: $logPath"
