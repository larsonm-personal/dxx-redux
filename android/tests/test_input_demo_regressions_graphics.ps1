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
    [string]$DemoRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [int]$TimeoutSeconds = 180,
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
