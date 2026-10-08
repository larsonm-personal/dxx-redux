#!/usr/bin/env pwsh
# Actual GLES texture ownership and enhanced/native draw interleaving
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
$mode = if ($Baseline) { 'bindings-baseline' } else { 'bindings' }
$expected = if ($Baseline) { 68 } else { 80 }
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture texture_labels -Mode $mode -ExpectedCases $expected @arguments
