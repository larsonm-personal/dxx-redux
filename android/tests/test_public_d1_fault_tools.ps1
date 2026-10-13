#!/usr/bin/env pwsh
# Public fault oracles, identity/trajectory checks and fail-fast capture tooling
$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path $PSScriptRoot) 'helpers/test_host_platform.ps1')
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for public effect phase tests' }
$prefix = @($python.PrefixArguments)
foreach ($pattern in @('test_check_public_effect_phase.py', 'test_check_public_mine_blast.py', 'test_check_public_fan_transparency.py', 'test_capture_public_d1_fault_probes.py', 'test_public_d1_fault_corpus.py', 'test_public_d1_fan_corpus.py')) {
    if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot $pattern) -PathType Leaf)) { throw "Missing public D1 fault test module: $pattern" }
    & $python.Path @prefix -m unittest discover -s $PSScriptRoot -p $pattern
    if ($LASTEXITCODE -ne 0) { throw "Public D1 fault tooling tests failed: $pattern" }
}
Write-Output 'PASS: public D1 fault oracles, causal controls and fail-fast capture guards'
