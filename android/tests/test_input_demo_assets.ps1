#!/usr/bin/env pwsh
# Asset-free tests for common replay asset verification and fail-closed selection
param()
$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path $PSScriptRoot) 'helpers/test_host_platform.ps1')
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for asset fingerprint tests' }
$prefix = @($python.PrefixArguments)
& $python.Path @prefix (Join-Path $PSScriptRoot 'test_input_demo_assets.py')
if ($LASTEXITCODE -ne 0) { throw 'Input-demo asset verification tests failed' }
