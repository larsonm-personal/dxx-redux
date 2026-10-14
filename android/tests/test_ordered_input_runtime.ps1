#!/usr/bin/env pwsh
# Native ordered-input phases and causal replay failures in both runtime backends
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$D1Exe,
    [Parameter(Mandatory)][string]$D2Exe,
    [Parameter(Mandatory)][string]$D1Headless,
    [Parameter(Mandatory)][string]$D2Headless,
    [Parameter(Mandatory)][string]$D1Graphics,
    [Parameter(Mandatory)][string]$D2Graphics,
    [string]$D1DataDir,
    [string]$D2DataDir,
    [string]$Output,
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 120,
    [ValidateRange(0.01, 1048576)][double]$MinimumFreeSpaceGB = 4
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/shareware_test_data.ps1')
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$D1DataDir = Resolve-SharewareTestData -RepoRoot $repoRoot -Game d1 -DataDir $D1DataDir
$D2DataDir = Resolve-SharewareTestData -RepoRoot $repoRoot -Game d2 -DataDir $D2DataDir
$D1Exe = (Resolve-Path -LiteralPath $D1Exe).Path
$D2Exe = (Resolve-Path -LiteralPath $D2Exe).Path
if (-not $Output) { $Output = Join-Path $repoRoot "temp/ordered_input_runtime_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')" }
$Output = [IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $Output) { throw "Use a new output directory: $Output" }
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $Output -MinimumFreeSpaceGB $MinimumFreeSpaceGB
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for ordered input verification' }
$prefix = @($python.PrefixArguments)
$pwsh = (Get-Process -Id $PID).Path
& $python.Path @prefix (Join-Path $PSScriptRoot 'check_ordered_input_runtime.py') --capture-d1 $D1Exe --capture-d2 $D2Exe `
    --headless-d1 ([IO.Path]::GetFullPath($D1Headless)) --headless-d2 ([IO.Path]::GetFullPath($D2Headless)) `
    --graphics-d1 ([IO.Path]::GetFullPath($D1Graphics)) --graphics-d2 ([IO.Path]::GetFullPath($D2Graphics)) `
    --data-d1 $D1DataDir --data-d2 $D2DataDir --output $Output --timeout $TimeoutSeconds --pwsh $pwsh `
    --minimum-free-gb $MinimumFreeSpaceGB.ToString([Globalization.CultureInfo]::InvariantCulture)
if ($LASTEXITCODE -ne 0) { throw "Native ordered input qualification failed; see $Output" }
Write-Output 'PASS: native D1/D2 ordered-input effects and causal negative controls on both backends'
