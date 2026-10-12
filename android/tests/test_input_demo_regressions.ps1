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
    [ValidateSet('recorded', 'public')][string]$Corpus = 'recorded',
    [switch]$NativeOnly,
    [string]$D1Exe,
    [string]$D2Exe,
    [string]$D1DataDir,
    [string]$D2DataDir,
    [ValidateRange(0.01, 1048576)][double]$MinimumFreeSpaceGB = 4,
    [string]$DemoRoot,
    [string[]]$DemoFileName,
    [string]$ResultArchiveRoot,
    [string]$ReferenceResultRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [ValidateSet('all', 'd1', 'd2')]
    [string]$RecordedGame = 'all',
    [ValidateSet('headless', 'graphics')]
    [string]$RunMode = 'headless',
    [int]$TimeoutSeconds = 180,
    [switch]$D1InD2,
    [switch]$ListOnly,
    [switch]$StopOnFirstFailure
)

$ErrorActionPreference = 'Stop'

if ($Corpus -eq 'public') {
    if ($D1InD2 -or $Game -ne 'auto' -or $RecordedGame -ne 'all' -or $DemoFileName -or $ReferenceResultRoot -or $ListOnly -or $StopOnFirstFailure) {
        throw 'Public qualification runs the complete admitted corpus; use the single replay runner for individual investigations'
    }
    $public = @{
        RunMode = $RunMode
        NativeOnly = $NativeOnly
        TimeoutSeconds = $TimeoutSeconds
        MinimumFreeSpaceGB = $MinimumFreeSpaceGB
    }
    foreach ($name in @('D1Exe', 'D2Exe', 'D1DataDir', 'D2DataDir')) {
        $value = Get-Variable -Name $name -ValueOnly
        if ($value) { $public[$name] = $value }
    }
    if ($DemoRoot) { $public.CorpusDirectory = $DemoRoot }
    if ($ResultArchiveRoot) { $public.Output = $ResultArchiveRoot }
    & "$PSScriptRoot/test_compact_public_replays.ps1" @public
    exit $LASTEXITCODE
}
if ($NativeOnly -or $D1Exe -or $D2Exe -or $D1DataDir -or $D2DataDir) {
    throw 'Native-only qualification, explicit executables and public data directories require -Corpus public'
}

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
if ($DemoFileName) {
    $forwardedParams.DemoFileName = $DemoFileName
}
if ($ResultArchiveRoot) {
    $forwardedParams.ResultArchiveRoot = $ResultArchiveRoot
}
if ($ReferenceResultRoot) {
    $forwardedParams.ReferenceResultRoot = $ReferenceResultRoot
}
if ($StopOnFirstFailure) {
    $forwardedParams.StopOnFirstFailure = $true
}
if ($D1InD2) {
    $forwardedParams.D1InD2 = $true
}
if ($ListOnly) {
    $forwardedParams.ListOnly = $true
}

& "$PSScriptRoot\run_input_demo_regressions.ps1" @forwardedParams
exit $LASTEXITCODE
