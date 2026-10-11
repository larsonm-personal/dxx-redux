#!/usr/bin/env pwsh
# Real pack priority, mask ownership and mission overrides through actual GLES uploads
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [string]$PackDirectory,
    [switch]$NoEngineBuild
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (!$PackDirectory) { $PackDirectory = Join-Path $repoRoot 'game_data/mods/d2x-xl' }
$dataFiles = @()
foreach ($name in @('DESCENT.PIG', 'DESCENT.HOG', 'DESCENT2.HOG', 'DESCENT2.HAM', 'DESCENT2.S22', 'GROUPA.PIG')) {
    $dataFiles += Join-Path $repoRoot "game_data_to_copy_to_emulator/temp/$name"
}
foreach ($game in @('d1', 'd2')) {
    $dataFiles += Join-Path $repoRoot "game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/uu${game}tp-textures.dxa"
    $dataFiles += Join-Path $PackDirectory "$game-hires-256-textures-ktx2.dxa"
}
$fixtureParams = @{} + $PSBoundParameters
$fixtureParams.Remove('PackDirectory')
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture mod_renderer -ExpectedCases 10 -ExpectedD2Cases 21 -DataFiles $dataFiles @fixtureParams
