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
    [switch]$CompareStateTrace
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

Write-Host 'Headless stage: prefer the dedicated D2 headless console runner; unsupported cases fall back to normal replay'
& $pwsh @args
exit $LASTEXITCODE