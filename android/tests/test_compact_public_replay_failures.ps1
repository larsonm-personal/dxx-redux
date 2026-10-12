#!/usr/bin/env pwsh
# Real-engine bad controls and wrong expected output, through both public wrappers
[CmdletBinding()]
param(
    [ValidateSet('headless', 'graphics')][string]$RunMode = 'headless',
    [Parameter(Mandatory)][string]$D1Exe,
    [Parameter(Mandatory)][string]$D2Exe,
    [string]$D1DataDir,
    [string]$D2DataDir,
    [string]$CorpusDirectory,
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
if (-not $CorpusDirectory) { $CorpusDirectory = Join-Path $PSScriptRoot 'fixtures/public_level_replays' }
$CorpusDirectory = (Resolve-Path -LiteralPath $CorpusDirectory).Path
if (-not $Output) { $Output = Join-Path $repoRoot "temp/compact_public_replay_failures_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')" }
$Output = [IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $Output) { throw "Use a new output directory: $Output" }
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $Output -MinimumFreeSpaceGB $MinimumFreeSpaceGB
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for compact public negative controls' }
$prefix = @($python.PrefixArguments)
$pwsh = (Get-Process -Id $PID).Path
& $python.Path @prefix (Join-Path $PSScriptRoot 'check_compact_public_replay_failures.py') --repo $repoRoot --corpus $CorpusDirectory --output $Output `
    --run-mode $RunMode --data-d1 $D1DataDir --data-d2 $D2DataDir --exe-d1 $D1Exe --exe-d2 $D2Exe --pwsh $pwsh `
    --timeout $TimeoutSeconds --minimum-free-gb $MinimumFreeSpaceGB.ToString([Globalization.CultureInfo]::InvariantCulture)
if ($LASTEXITCODE -ne 0) { throw "Real-engine negative control did not prove the expected failure: $Output" }
Write-Output "PASS: every public scene rejected bad controls and wrong live-frame expectations through the $RunMode backend and complete regression wrapper chain"
