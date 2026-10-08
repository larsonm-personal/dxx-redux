#!/usr/bin/env pwsh
# Actual preview producer, callback and public state with controlled OpenSL FIFO
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture cd_preview_completion -ExpectedCases 11 @PSBoundParameters
