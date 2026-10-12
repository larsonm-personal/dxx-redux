#!/usr/bin/env pwsh
# Actual engine source metadata and UUD save adaptation
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot)
$data = @('DESCENT2.HAM', 'DESCENT2.S22', 'GROUPA.PIG', 'DESCENT.HOG', 'DESCENT.PIG') | ForEach-Object { Join-Path $repo "game_data_to_copy_to_emulator/temp/$_" }
$data += Join-Path $repo 'game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/uud1tp-textures.dxa'
$data += Join-Path $repo 'game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/uud2tp-textures.dxa'
$data += Join-Path $repo 'game_data/mods/xfing/d2x_sp/uud2sp-sounds.dxa'
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture mod_save -ExpectedCases 7 -ExpectedD2Cases 19 -DataFiles $data @PSBoundParameters
