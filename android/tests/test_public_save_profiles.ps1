#!/usr/bin/env pwsh
# Synthetic admission checks; the separate integration runner owns native captures
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../helpers/test_host_platform.ps1')
$python = Resolve-RegressionPythonCommand
if (-not $python) { throw 'Python 3 is required for save evidence admission checks' }
$prefix = @($python.PrefixArguments)
& $python.Path @prefix (Join-Path $PSScriptRoot 'test_public_save_continuation.py')
if ($LASTEXITCODE -ne 0) { throw 'Ordinary save evidence admission checks failed' }
