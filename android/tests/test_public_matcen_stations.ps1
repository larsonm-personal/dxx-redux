#!/usr/bin/env pwsh
# Explicit controlled public station-loop coverage; no normal player route claim
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$D1Exe,
    [Parameter(Mandatory)][string]$D2Exe,
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
if (-not $Output) { $Output = Join-Path $repoRoot "temp/public_matcen_stations_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')" }
$Output = [IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $Output) { throw "Use a new output directory: $Output" }
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $Output -MinimumFreeSpaceGB $MinimumFreeSpaceGB
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for public matcen verification' }
$prefix = @($python.PrefixArguments)
& $python.Path @prefix (Join-Path $PSScriptRoot 'run_public_matcen_stations.py') --exe-d1 $D1Exe --exe-d2 $D2Exe `
    --data-d1 $D1DataDir --data-d2 $D2DataDir --output $Output --timeout $TimeoutSeconds
if ($LASTEXITCODE -ne 0) { throw "Controlled public matcen production failed; see $Output" }
Write-Output 'PASS: controlled native public matcen production and suppression twice, with nonvacuous activation negatives'
