#!/usr/bin/env pwsh
# Verify that generated D2X-XL sounds match each native loader contract

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$packLibraryPath = Join-Path $repoRoot "game_data/mods/d2x-xl/d2xxl_pack_lib.ps1"
$converterPath = Join-Path $repoRoot "game_data/mods/d2x-xl/convert_d2xxl_sounds.ps1"
$d1LoaderPath = Join-Path $repoRoot "d1/main/bmread.c"
$d2LoaderPath = Join-Path $repoRoot "d2/main/bmread.c"

. $packLibraryPath

$d1 = Get-D2xxlSoundFormat -GameId "d1"
$d2 = Get-D2xxlSoundFormat -GameId "d2"
if ($d1.SampleRate -ne 11025 -or $d1.Extension -ne ".raw") {
    throw "D1 generated sound contract does not match 11025 Hz .raw"
}
if ($d2.SampleRate -ne 22050 -or $d2.Extension -ne ".r22") {
    throw "D2 generated sound contract does not match 22050 Hz .r22"
}
if ((Get-D2xxlSoundEntryPath -GameId "d1" -BaseName "laser") -ne "Sounds/laser.raw") {
    throw "D1 sound entry path does not match its loader"
}
if ((Get-D2xxlSoundEntryPath -GameId "d2" -BaseName "laser") -ne "Sounds/laser.r22") {
    throw "D2 sound entry path does not match its loader"
}

$converter = Get-Content -LiteralPath $converterPath -Raw
$d1Loader = Get-Content -LiteralPath $d1LoaderPath -Raw
$d2Loader = Get-Content -LiteralPath $d2LoaderPath -Raw
if ($converter -notmatch 'Get-D2xxlSoundEntryPath -GameId \$GameId' -or
    $converter -notmatch 'Convert-ToUnsignedMonoPcm -Wav \$wavData -TargetRate \$soundFormat\.SampleRate') {
    throw "Sound converter bypasses the shared per-game output contract"
}
if ($d1Loader -notmatch 'sprintf\(\s*rawname,\s*"Sounds/%s\.raw"') {
    throw "D1 loader no longer admits Sounds/{name}.raw"
}
if ($d2Loader -notmatch 'SndDigiSampleRate==SAMPLE_RATE_22K\) \? "r22" : "raw"') {
    throw "D2 loader no longer selects .r22 in 22 kHz mode"
}

$unsupportedFailed = $false
try {
    Get-D2xxlSoundFormat -GameId "unknown" | Out-Null
} catch {
    $unsupportedFailed = $true
}
if (-not $unsupportedFailed) {
    throw "Unknown game IDs must not receive an output sound format"
}

Write-Host "PASS: generated D2X-XL sound formats match the D1 and D2 loaders"
