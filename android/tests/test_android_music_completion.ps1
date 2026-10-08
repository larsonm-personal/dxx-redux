#!/usr/bin/env pwsh
# Actual music producer, callback and completion lifecycle in an isolated process
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture music_completion -ExpectedCases 21 @PSBoundParameters
