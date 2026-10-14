#!/usr/bin/env pwsh
# Live recorded grants, compact oracles, and causal controls on both native backends
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$D1BuildDir,
    [Parameter(Mandatory)][string]$D2BuildDir,
    [string]$D1DataDir,
    [string]$D2DataDir,
    [string]$Output,
    [string]$Revision,
    [ValidateRange(1, 600)][int]$TimeoutSeconds = 120,
    [ValidateRange(0.01, 1048576)][double]$MinimumFreeSpaceGB = 4
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/shareware_test_data.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$D1DataDir = Resolve-SharewareTestData -RepoRoot $repoRoot -Game d1 -DataDir $D1DataDir
$D2DataDir = Resolve-SharewareTestData -RepoRoot $repoRoot -Game d2 -DataDir $D2DataDir
$D1BuildDir = (Resolve-Path -LiteralPath $D1BuildDir).Path
$D2BuildDir = (Resolve-Path -LiteralPath $D2BuildDir).Path
if (-not $Revision) { $Revision = (& git -C $repoRoot rev-parse HEAD).Trim(); if ($LASTEXITCODE -ne 0) { throw 'Cannot resolve source revision' } }
if (-not $Output) { $Output = Join-Path $repoRoot "temp/all_weapons_runtime_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')" }
$Output = [IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $Output) { throw "Use a new output directory: $Output" }
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $Output -MinimumFreeSpaceGB $MinimumFreeSpaceGB
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for all-weapons verification' }
$prefix = @($python.PrefixArguments)
$arguments = @('--repo', $repoRoot, '--output', $Output, '--revision', $Revision,
    '--build-d1', $D1BuildDir, '--build-d2', $D2BuildDir, '--data-d1', $D1DataDir, '--data-d2', $D2DataDir,
    '--pwsh', (Get-Process -Id $PID).Path, '--timeout', "$TimeoutSeconds", '--minimum-free-gb', $MinimumFreeSpaceGB.ToString([Globalization.CultureInfo]::InvariantCulture))
foreach ($game in @('d1', 'd2')) {
    $build = if ($game -eq 'd1') { $D1BuildDir } else { $D2BuildDir }
    foreach ($role in @('capture', 'graphics', 'headless')) {
        $stem = switch ($role) { 'capture' { 'test_upstream_compat' }; 'graphics' { "${game}x-redux" }; 'headless' { "dxx-redux-$game-headless" } }
        $path = $null
        foreach ($name in (Get-RegressionHostExecutableNames -BaseName $stem)) {
            $candidate = Join-Path (Join-Path $build 'main') $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { $path = $candidate; break }
        }
        if (-not $path) { throw "Missing $role executable in $build/main" }
        $arguments += @("--$role-$game", $path)
    }
}
& $python.Path @prefix (Join-Path $PSScriptRoot 'check_all_weapons_runtime.py') @arguments
if ($LASTEXITCODE -ne 0) { throw "Recorded all-weapons qualification failed; see $Output" }
Write-Output 'PASS: native recorded grants, compact live oracles and causal replay controls'
