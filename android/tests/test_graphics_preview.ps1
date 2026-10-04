#!/usr/bin/env pwsh
# Run isolated level and robot preview eligibility cases in both engines
[CmdletBinding()]
param(
    [ValidateSet('d1', 'd2')][string]$Game,
    [ValidateSet('level', 'robot')][string]$Preview,
    [string]$Serial
)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot '../helpers/run_graphics_preview_tests.ps1') @PSBoundParameters
exit 0
