#!/usr/bin/env pwsh
# Production automap updater with controlled adoption and clock state
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture automap_route_policy -ExpectedCases 68 @PSBoundParameters
