#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Discovers and validates CD extract regression specs.
#>

$ErrorActionPreference = 'Stop'

& "$PSScriptRoot\validate_extract_regression_specs.ps1"
exit $LASTEXITCODE
