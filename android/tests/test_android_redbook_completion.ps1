#!/usr/bin/env pwsh
# Actual Redbook producer, tail drain and public completion lifecycle
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture redbook_completion -ExpectedCases 10 @PSBoundParameters
