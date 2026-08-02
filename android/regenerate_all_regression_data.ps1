#!/usr/bin/env pwsh
<#
.SYNOPSIS
  Regenerates all checked-in regression data.

.DESCRIPTION
  Runs the canonical CD regression, fingerprint/AcoustID, and mission metadata
  regeneration wrappers. Each wrapper runs in a child PowerShell process. The
  workflow stops immediately if a wrapper is missing or returns a nonzero exit
  code.

.EXAMPLE
  .\android\regenerate_all_regression_data.ps1
#>

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$script:RepoRoot = Split-Path $PSScriptRoot -Parent

function Get-RegressionDataStages {
    param([string]$RepoRoot)

    return @(
        [pscustomobject]@{
            Name = 'CD extraction regression data'
            Script = Join-Path $RepoRoot 'game_data\run_all_cd_regressions.ps1'
            Arguments = @('-RefreshOracle')
        },
        [pscustomobject]@{
            Name = 'Disc and music fingerprint data'
            Script = Join-Path $RepoRoot 'game_data\update_all_fingerprints.ps1'
            Arguments = @('-Force')
        },
        [pscustomobject]@{
            Name = 'Mission level metadata'
            Script = Join-Path $RepoRoot 'android\helpers\regenerate_all_mission_metadata.ps1'
            Arguments = @()
        }
    )
}

function Invoke-RegressionDataStages {
    param(
        [object[]]$Stages,
        [string]$PowerShellPath = (Get-Process -Id $PID).Path
    )

    for ($index = 0; $index -lt $Stages.Count; $index++) {
        $stage = $Stages[$index]
        if (-not (Test-Path -LiteralPath $stage.Script -PathType Leaf)) {
            throw "Regression data stage script not found: $($stage.Script)"
        }

        Write-Host ''
        Write-Host '============================================================' -ForegroundColor White
        Write-Host "  REGENERATION STAGE $($index + 1)/$($Stages.Count): $($stage.Name)" -ForegroundColor Cyan
        Write-Host '============================================================' -ForegroundColor White

        & $PowerShellPath -NoProfile -NonInteractive -File $stage.Script @($stage.Arguments)
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) {
            throw "Regression data stage '$($stage.Name)' failed with exit code $exitCode"
        }
    }
}

if ($MyInvocation.InvocationName -ne '.') {
    try {
        $stages = @(Get-RegressionDataStages -RepoRoot $script:RepoRoot)
        Invoke-RegressionDataStages -Stages $stages
    } catch {
        Write-Host ''
        Write-Host "FAILED: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }

    Write-Host ''
    Write-Host 'All regression data regenerated successfully' -ForegroundColor Green
}
