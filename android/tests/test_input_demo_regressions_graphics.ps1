#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Runs committed input-demo regression fixtures in unattended graphics mode.

.DESCRIPTION
    Wraps run_input_demo_regressions.ps1 with explicit non-interactive defaults
    so run_all_tests.ps1 can cover the committed .dximdemo corpus in the full
    game binary as well as the headless pass.

.EXAMPLE
    .\test_input_demo_regressions_graphics.ps1
#>

param(
    [ValidateSet('recorded', 'public')][string]$Corpus = 'recorded',
    [switch]$NativeOnly,
    [string]$D1Exe,
    [string]$D2Exe,
    [string]$D1DataDir,
    [string]$D2DataDir,
    [ValidateRange(0.01, 1048576)][double]$MinimumFreeSpaceGB = 4,
    [string]$DemoRoot,
    [switch]$SharewareOnly,
    [switch]$AllowUnverifiedAssets,
    [string[]]$DemoFileName,
    [string]$ResultArchiveRoot,
    [string]$ReferenceResultRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [ValidateSet('all', 'd1', 'd2')]
    [string]$RecordedGame = 'all',
    [int]$TimeoutSeconds = 180,
    [switch]$D1InD2,
    [switch]$ListOnly,
    [switch]$StopOnFirstFailure
)

$ErrorActionPreference = 'Stop'

$forwardedParams = @{}
foreach ($key in $PSBoundParameters.Keys) {
    $forwardedParams[$key] = $PSBoundParameters[$key]
}
$forwardedParams.RunMode = 'graphics'

& "$PSScriptRoot\test_input_demo_regressions.ps1" @forwardedParams
exit $LASTEXITCODE
