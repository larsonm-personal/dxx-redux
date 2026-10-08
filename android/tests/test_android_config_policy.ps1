#!/usr/bin/env pwsh
# Actual engine integration in an isolated native Android process
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture config_policy -ExpectedCases 58 @PSBoundParameters
