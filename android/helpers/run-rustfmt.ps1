#!/usr/bin/env pwsh
param([switch]$Check, [string[]]$Paths)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $PSScriptRoot 'code-quality-files.ps1')
$Paths = @(Get-CodeQualityScriptPaths -InputPaths $Paths -RemainingPaths @($args) -ExplicitScope ($PSBoundParameters.ContainsKey('Paths')))
$files = @(Get-CodeQualityScopedFiles -RepoRoot $repoRoot -RootPath $repoRoot -InputPaths $Paths -ValidExtensions @('.rs'))
if (-not $files.Count) { Write-Host 'No Rust files in scope'; exit 0 }
$failed = $false
$toolchain = Get-CodeQualityToolVersion -RepoRoot $repoRoot -Name 'RUSTFMT_TOOLCHAIN'
foreach ($file in $files) {
    # Do not let rustfmt follow mod declarations into files outside the selected scope
    $parameters = @('run', $toolchain, 'rustfmt', '--edition', '2021', '--config', 'skip_children=true,newline_style=Unix')
    if ($Check) { $parameters += '--check' }
    & rustup @parameters $file.FullName
    if ($LASTEXITCODE -ne 0) { $failed = $true }
}
if ($failed) { exit 1 }
exit 0
