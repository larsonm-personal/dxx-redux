#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Runs committed input-demo regression fixtures in unattended mode.

.DESCRIPTION
    Wraps run_input_demo_regressions.ps1 with explicit non-interactive defaults
    so run_all_tests.ps1 can include the committed .dximdemo corpus. Defaults
    to headless mode; the graphics wrapper delegates here with RunMode set.

.EXAMPLE
    .\test_input_demo_regressions.ps1
#>

param(
    [string]$DemoRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [ValidateSet('all', 'd1', 'd2')]
    [string]$RecordedGame = 'all',
    [ValidateSet('headless', 'graphics')]
    [string]$RunMode = 'headless',
    [int]$TimeoutSeconds = 180,
    [switch]$D1InD2,
    [switch]$StopOnFirstFailure
)

$ErrorActionPreference = 'Stop'

$forwardedParams = @{
    Game = $Game
    RecordedGame = $RecordedGame
    Mode = 'accelerated'
    RunMode = $RunMode
    TimeoutSeconds = $TimeoutSeconds
}
if ($DemoRoot) {
    $forwardedParams.DemoRoot = $DemoRoot
}
if ($StopOnFirstFailure) {
    $forwardedParams.StopOnFirstFailure = $true
}
if ($D1InD2) {
    $forwardedParams.D1InD2 = $true
}

& "$PSScriptRoot\run_input_demo_regressions.ps1" @forwardedParams
exit $LASTEXITCODE
