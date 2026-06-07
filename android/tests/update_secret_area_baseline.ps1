#!/usr/bin/env pwsh
param(
    [ValidateSet('both', 'd1', 'd2')]
    [string]$Game = 'both',
    [string]$BaselinePath,
    [string]$OutputDir,
    [Alias('HogDir')]
    [string]$DataDir,
    [string]$D1DataDir,
    [string]$D2DataDir,
    [string]$D1Exe,
    [string]$D2Exe,
    [switch]$BuildBeforeRun,
    [switch]$RequireAssets
)

$ErrorActionPreference = 'Stop'

$argsList = @(
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', (Join-Path $PSScriptRoot 'test_secret_area_baseline.ps1'),
    '-Game', $Game,
    '-UpdateBaseline'
)

if ($BaselinePath) { $argsList += @('-BaselinePath', $BaselinePath) }
if ($OutputDir) { $argsList += @('-OutputDir', $OutputDir) }
if ($DataDir) { $argsList += @('-DataDir', $DataDir) }
if ($D1DataDir) { $argsList += @('-D1DataDir', $D1DataDir) }
if ($D2DataDir) { $argsList += @('-D2DataDir', $D2DataDir) }
if ($D1Exe) { $argsList += @('-D1Exe', $D1Exe) }
if ($D2Exe) { $argsList += @('-D2Exe', $D2Exe) }
if ($BuildBeforeRun) { $argsList += '-BuildBeforeRun' }
if ($RequireAssets) { $argsList += '-RequireAssets' }

$pwsh = (Get-Process -Id $PID).Path
& $pwsh @argsList
exit $LASTEXITCODE
