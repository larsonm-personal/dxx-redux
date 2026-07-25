#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Regenerates and validates all CD extraction regression data.

.DESCRIPTION
  Runs host extraction, structural spec validation, and the complete Android
  extraction and launch suite against the existing regression specs. Each
  stage runs in a child PowerShell process, and the workflow stops immediately
  when a stage returns a nonzero exit code. Use -RefreshOracle to regenerate
  specs as an explicit maintenance operation.

.PARAMETER NoForce
  Reuse existing host extraction data when available.

.PARAMETER RefreshOracle
  Explicitly regenerate regression specs from the current host extraction.
  Normal regression runs compare current extraction against the existing specs.

.PARAMETER SkipLaunch
  Run extraction and file verification without launching each game. This
  reduced-coverage mode does not update persisted regression results.

.EXAMPLE
  .\game_data\run_all_cd_regressions.ps1
#>
param(
    [switch]$NoForce,
    [switch]$RefreshOracle,
    [switch]$SkipLaunch
)

$ErrorActionPreference = 'Stop'
$script:RepoRoot = Split-Path $PSScriptRoot -Parent

function Get-CdRegressionStages {
    param(
        [string]$RepoRoot,
        [switch]$NoForce,
        [switch]$RefreshOracle,
        [switch]$SkipLaunch
    )

    $forceArgs = if ($NoForce) { @() } else { @('-Force') }
    $testArgs = @('-All')
    if ($SkipLaunch) {
        $testArgs += '-SkipLaunch'
    }

    $stages = @(
        [pscustomobject]@{
            Name = 'Extract all CD images'
            Script = Join-Path $RepoRoot 'game_data\extract_all_cds.ps1'
            Arguments = $forceArgs
        }
    )
    if ($RefreshOracle) {
        $stages += [pscustomobject]@{
            Name = 'Refresh regression specs'
            Script = Join-Path $RepoRoot 'game_data\generate_regression_specs.ps1'
            Arguments = @('-Force')
        }
    }
    $stages += @(
        [pscustomobject]@{
            Name = 'Validate regression specs'
            Script = Join-Path $RepoRoot 'android\tests\validate_extract_regression_specs.ps1'
            Arguments = @()
        },
        [pscustomobject]@{
            Name = 'Run all extraction regressions'
            Script = Join-Path $RepoRoot 'android\tests\test_all_extracts.ps1'
            Arguments = $testArgs
        }
    )
    return $stages
}

function Invoke-CdRegressionStages {
    param(
        [object[]]$Stages,
        [string]$PowerShellPath = (Get-Process -Id $PID).Path
    )

    for ($index = 0; $index -lt $Stages.Count; $index++) {
        $stage = $Stages[$index]
        if (-not (Test-Path -LiteralPath $stage.Script -PathType Leaf)) {
            throw "CD regression stage script not found: $($stage.Script)"
        }

        Write-Host ''
        Write-Host '============================================================' -ForegroundColor White
        Write-Host "  CD REGRESSION STAGE $($index + 1)/$($Stages.Count): $($stage.Name)" -ForegroundColor Cyan
        Write-Host '============================================================' -ForegroundColor White

        & $PowerShellPath -NoProfile -NonInteractive -File $stage.Script @($stage.Arguments)
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) {
            throw "CD regression stage '$($stage.Name)' failed with exit code $exitCode"
        }
    }
}

if ($MyInvocation.InvocationName -ne '.') {
    $stages = @(
        Get-CdRegressionStages -RepoRoot $script:RepoRoot -NoForce:$NoForce `
            -RefreshOracle:$RefreshOracle -SkipLaunch:$SkipLaunch
    )
    try {
        Invoke-CdRegressionStages -Stages $stages
    } catch {
        Write-Host ''
        Write-Host "FAILED: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }

    Write-Host ''
    Write-Host 'All CD regression stages passed' -ForegroundColor Green
}
