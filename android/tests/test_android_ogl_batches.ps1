#!/usr/bin/env pwsh
# Actual engine glyph and ordered line rendering in isolated GLES contexts
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
$mode = if ($Baseline) { 'batches-baseline' } else { 'batches' }
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture texture_labels -Mode $mode -ExpectedCases 42 @arguments
