#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_native_host_unit_tests
# The native owner runs this CTest target for both games
param(
    [ValidateSet('d1', 'd2')][string]$Game = 'd2',
    [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_host_platform.ps1')
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
if (-not $NoBuild) {
    Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target $Game
}
$testDir = Join-Path $repoRoot "build$Game/tests"
$executable = $null
foreach ($name in (Get-RegressionHostExecutableNames -BaseName 'test_coop_transition_policy')) {
    $candidate = Join-Path $testDir $name
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $executable = $candidate
        break
    }
}
if (-not $executable) { throw "Co-op transition test executable missing in $testDir" }
& $executable
if ($LASTEXITCODE -ne 0) { throw "Co-op transition policy test failed: $LASTEXITCODE" }
