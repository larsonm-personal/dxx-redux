#!/usr/bin/env pwsh
# Actual paired mission loaders with pinned game assets in isolated native processes
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/standard_game_data.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$dataFiles = foreach ($game in @('d1', 'd2')) {
    $names = if ($game -eq 'd1') { @('descent.hog') } else { @('descent2.ham', 'descent2.s22') }
    $dependencies = @(Get-StandardGameDataDeps | Where-Object file -in $names)
    $source = Resolve-StandardGameDataDirectory -Candidates @(Get-StandardGameDataCandidates -RepoRoot $repoRoot -Game $game) -Dependencies $dependencies -Label $game
    foreach ($name in $names) { Join-Path $source.Path $name }
}
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture secret_origins -ExpectedCases 12 -DataFiles $dataFiles @PSBoundParameters
