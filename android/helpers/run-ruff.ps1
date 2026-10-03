#!/usr/bin/env pwsh
param([switch]$Check, [string[]]$Paths)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'code-quality-files.ps1')
$Paths = @(Get-CodeQualityScriptPaths -InputPaths $Paths -RemainingPaths @($args) -ExplicitScope ($PSBoundParameters.ContainsKey('Paths')))
$files = @(Get-CodeQualityScopedFiles -RepoRoot $repoRoot -RootPath $repoRoot -InputPaths $Paths -ValidExtensions @('.py'))
if (-not $files.Count) { Write-Host 'No Python files in scope'; exit 0 }
$pythonPackages = Join-Path $repoRoot 'android/temp/code-quality/python'
$oldPythonPath = $env:PYTHONPATH
$ruffVersion = Get-CodeQualityToolVersion -RepoRoot $repoRoot -Name 'RUFF_VERSION'
$python = Get-CodeQualityPython
try {
    $env:PYTHONPATH = $pythonPackages
    $version = & $python -m ruff --version
    if ($LASTEXITCODE -ne 0 -or $version -ne "ruff $ruffVersion") { throw 'Pinned Ruff is missing; run android/run-code-quality.ps1 -InstallTools with Python in PATH' }
    $failed = $false
    for ($index = 0; $index -lt $files.Count; $index += 30) {
        $batch = @($files[$index..([Math]::Min($index + 29, $files.Count - 1))] | ForEach-Object { $_.FullName })
        $parameters = @('format', '--target-version', 'py311', '--line-length', '120')
        if ($Check) { $parameters += '--check' }
        & $python -m ruff @parameters @batch
        if ($LASTEXITCODE -ne 0) { $failed = $true }
    }
    if ($failed) { exit 1 }
} finally { $env:PYTHONPATH = $oldPythonPath }
exit 0
