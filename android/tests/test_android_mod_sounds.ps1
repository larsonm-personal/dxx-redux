#!/usr/bin/env pwsh
# Real UUD2SP banks, sound assignments, mixer consumption and archive priority
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot)
$data = @('DESCENT2.HAM', 'DESCENT2.S22', 'GROUPA.PIG') | ForEach-Object { Join-Path $repo "game_data_to_copy_to_emulator/temp/$_" }
$data += Join-Path $repo 'game_data/mods/xfing/d2x_sp/uud2sp-sounds.dxa'
$data += Join-Path $repo 'game_data/mods/xfing/d2x_sp/UUD2SP1_4.dxa'
$data += Join-Path $repo 'game_data/mods/d2x-xl/d2-hires-sounds.dxa'
& (Join-Path $repo 'game_data/mods/xfing/d2x_sp/verify-uud2sp-ham-patch-dxa.ps1')
& (Join-Path $repo 'game_data/mods/xfing/d2x_sp/verify-uud2sp-uud2tp-ham-composition.ps1')
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture mod_sounds -ExpectedCases 1 -ExpectedD2Cases 11 -DataFiles $data @PSBoundParameters
