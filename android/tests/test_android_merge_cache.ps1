#!/usr/bin/env pwsh
# Native cached-merge creation, reuse, eviction and cleanup in GLES
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$NativeLibraryDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture texture_labels -Mode merge-cache -ExpectedCases 124 @PSBoundParameters
