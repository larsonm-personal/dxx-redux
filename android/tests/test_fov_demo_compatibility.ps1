#!/usr/bin/env pwsh
# Own the FOV recording and classic playback script that needs a unique demo name
[CmdletBinding()]
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [string]$Serial = 'emulator-5554',
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot '../helpers/run_fov_demo_tests.ps1') @PSBoundParameters
exit 0
