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
    [switch]$ListOnly
)

$ErrorActionPreference = 'Stop'

$wrapper = Join-Path $PSScriptRoot 'run_input_demo_replay.ps1'
$pwsh = (Get-Process -Id $PID).Path
$args = @(
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', $wrapper,
    '-Game', $Game,
    '-Mode', $Mode,
    '-TimeoutSeconds', [string]$TimeoutSeconds,
    '-PreferHeadlessConsole',
    '-NoRender'
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
if ($ReuseSandbox) {
    $args += '-ReuseSandbox'
}
if ($KeepSandbox) {
    $args += '-KeepSandbox'
}
if ($ListOnly) {
    $args += '-ListOnly'
}

Write-Host 'Headless stage: prefer the console runner for D2 checkpoint demos and fall back to no-present replay otherwise'
& $pwsh @args
exit $LASTEXITCODE