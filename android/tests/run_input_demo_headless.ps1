#!/usr/bin/env pwsh
param(
    [string]$DemoPath,
    [string]$SearchRoot,
    [ValidateSet('auto', 'd1', 'd2')]
    [string]$Game = 'auto',
    [ValidateSet('prompt', 'realtime', 'accelerated')]
    [string]$Mode = 'accelerated',
    [int]$TimeoutSeconds = 300,
    [Alias('HogDir')]
    [string]$DataDir,
    [string]$Pilot,
    [switch]$ReuseSandbox,
    [switch]$KeepSandbox,
    [switch]$ListOnly,
    [string]$StateLogPath,
    [switch]$TraceState,
    [switch]$CompareStateTrace,
    [switch]$SkipExpectedChecks,
    [ValidateSet(1, 2)]
    [int]$HeadlessConsoleOutput = 1
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'input_demo_host_build_guard.ps1')
$wrapper = Join-Path $PSScriptRoot 'run_input_demo_replay.ps1'
$pwsh = (Get-Process -Id $PID).Path

if (-not $ListOnly) {
    $preflightGames = if ($Game -eq 'auto') {
        if ($DemoPath) {
            @(Get-InputDemoRecordedGameName -DemoPath $DemoPath)
        } else {
            @('d1', 'd2')
        }
    } else {
        @($Game)
    }

    foreach ($preflightGame in ($preflightGames | Sort-Object -Unique)) {
        $preferHeadless = $preflightGame -eq 'd2' -and $Mode -eq 'accelerated'
        Ensure-InputDemoGameBuild -RepoRoot $repoRoot -GameName $preflightGame -PreferHeadlessConsole:$preferHeadless
    }
}

$args = @(
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', $wrapper,
    '-Game', $Game,
    '-Mode', $Mode,
    '-TimeoutSeconds', [string]$TimeoutSeconds,
    '-HeadlessConsoleOutput', [string]$HeadlessConsoleOutput,
    '-PreferHeadlessConsole'
)

if ($DemoPath) {
    $args += @('-DemoPath', $DemoPath)
}
if ($SearchRoot) {
    $args += @('-SearchRoot', $SearchRoot)
}
if ($DataDir) {
    $args += @('-DataDir', $DataDir)
}
if ($Pilot) {
    $args += @('-Pilot', $Pilot)
}
if ($StateLogPath) {
    $args += @('-StateLogPath', $StateLogPath)
}
if ($ReuseSandbox) {
    $args += '-ReuseSandbox'
}
if ($KeepSandbox) {
    $args += '-KeepSandbox'
}
if ($ListOnly) {
    $args += '-ListOnly'
}
if ($TraceState) {
    $args += '-TraceState'
}
if ($CompareStateTrace) {
    $args += '-CompareStateTrace'
}
if ($SkipExpectedChecks) {
    $args += '-SkipExpectedChecks'
}

if ($HeadlessConsoleOutput -eq 2) {
    Write-Host 'Headless stage: prefer the dedicated D2 headless console runner; unsupported cases fall back to normal replay'
}
& $pwsh @args
exit $LASTEXITCODE