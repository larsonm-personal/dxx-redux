#!/usr/bin/env pwsh
# Stage the MP3 fixture before exercising launcher media controls
[CmdletBinding()]
param(
    [string]$Serial = 'emulator-5554',
    [string]$AudioFile,
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot '../helpers/test_launcher_media_controls.ps1') @PSBoundParameters
exit $LASTEXITCODE
