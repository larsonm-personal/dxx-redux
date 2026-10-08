#!/usr/bin/env pwsh
# Actual RGB/RGBA POT/NPOT enhanced-model mipmap rendering
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$NativeLibraryDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild,
    [switch]$Baseline
)
$arguments = @{} + $PSBoundParameters
$arguments.Remove('Baseline') | Out-Null
$mode = if ($Baseline) { 'mipmaps-baseline' } else { 'mipmaps' }
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture texture_labels -Mode $mode -ExpectedCases 16 @arguments
