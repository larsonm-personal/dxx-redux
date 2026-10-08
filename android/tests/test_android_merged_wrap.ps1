#!/usr/bin/env pwsh
# Actual merged-wall compositor clamp and GL state restoration integration
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
$mode = if ($Baseline) { 'merged-wrap-baseline' } else { 'merged-wrap' }
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture texture_labels -Mode $mode -ExpectedCases 240 @arguments
