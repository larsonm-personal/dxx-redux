#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Runs committed input-demo regression fixtures in unattended host mode.

.DESCRIPTION
    Wraps run_input_demo_regressions.ps1 with explicit non-interactive defaults
    so run_all_tests.ps1 can include the committed .dximdemo corpus.

.EXAMPLE
    .\test_input_demo_regressions.ps1
#>

param(
    [string]$DemoRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [int]$TimeoutSeconds = 180,
    [switch]$StopOnFirstFailure
)

$ErrorActionPreference = 'Stop'

$forwardedParams = @{
    Game = $Game
    Mode = 'accelerated'
    RunMode = 'headless'
    TimeoutSeconds = $TimeoutSeconds
}
if ($DemoRoot) {
    $forwardedParams.DemoRoot = $DemoRoot
}
if ($StopOnFirstFailure) {
    $forwardedParams.StopOnFirstFailure = $true
}

& "$PSScriptRoot\run_input_demo_regressions.ps1" @forwardedParams
exit $LASTEXITCODE