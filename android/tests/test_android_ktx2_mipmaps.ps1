#!/usr/bin/env pwsh
# Authored ETC2 levels, actual LOD sampling and device-limit fallback in both engines
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$NativeLibraryDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
$pack = Join-Path $PSScriptRoot '../../game_data/mods/d2x-xl/d2-hires-128-textures-ktx2.dxa'
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture texture_labels -Mode ktx2-mipmaps -ExpectedCases 48 -DataFiles $pack @PSBoundParameters
