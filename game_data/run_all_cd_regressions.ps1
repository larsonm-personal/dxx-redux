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
    [switch]$SkipLaunch,
    [ValidateRange(0.000001, 1.0)][double]$SampleFraction = 1.0,
    [ValidateRange(0, [int]::MaxValue)][int]$SampleSeed = 0
)

$ErrorActionPreference = 'Stop'
$script:RepoRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $script:RepoRoot 'android\helpers\runtime_targeted_sampling.ps1')
. (Join-Path $script:RepoRoot 'android\helpers\json5.ps1')

function New-CdRegressionSampleList {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][ValidateRange(0.000001, 1.0)][double]$Fraction,
        [Parameter(Mandatory)][int]$Seed
    )

    $specs = @(Get-ChildItem (Join-Path $RepoRoot 'game_data') -Recurse -Filter '*_regression.json5' -File |
            Sort-Object FullName | ForEach-Object {
                $spec = Read-Json5File $_.FullName
                [pscustomobject]@{
                    Name = $_.FullName
                    Path = $_.FullName
                    Type = if ($spec.source_type) { [string]$spec.source_type } else { 'unknown' }
                }
            })
    $selected = @()
    $groups = @($specs | Group-Object Type | Sort-Object Name)
    for ($index = 0; $index -lt $groups.Count; $index++) {
        $selected += Select-RuntimeFractionItems -Items @($groups[$index].Group) `
            -Fraction $Fraction -Seed ($Seed -bxor (501 + $index))
    }
    return @($selected | Select-Object -ExpandProperty Path -Unique)
}

function Get-CdRegressionStages {
    param(
        [string]$RepoRoot,
        [switch]$NoForce,
        [switch]$RefreshOracle,
        [switch]$SkipLaunch,
        [string]$SpecListPath
    )

    [string[]]$forceArgs = if ($NoForce) { @() } else { @('-Force') }
    $testArgs = @('-All', '-BuildAndInstall', '-RestartDevice')
    if ($SkipLaunch) {
        $testArgs += '-SkipLaunch'
    }
    if ($SpecListPath) {
        $forceArgs += @('-SpecListPath', $SpecListPath)
        $testArgs = @('-SpecListPath', $SpecListPath, '-BuildAndInstall', '-RestartDevice')
        if ($SkipLaunch) { $testArgs += '-SkipLaunch' }
    }

    $stages = @(
        [pscustomobject]@{
            Name = 'Extract all CD images'
            Script = Join-Path $RepoRoot 'game_data\extract_all_cds.ps1'
            Arguments = $forceArgs
        },
        [pscustomobject]@{
            Name = 'Extract all GOG installers'
            Script = Join-Path $RepoRoot 'game_data\extract_all_gog.ps1'
            Arguments = $forceArgs
        }
    )
    if ($RefreshOracle) {
        $stages += [pscustomobject]@{
            Name = 'Refresh regression specs'
            Script = Join-Path $RepoRoot 'game_data\generate_regression_specs.ps1'
            Arguments = @('-Force') + $(if ($SpecListPath) { @('-SpecListPath', $SpecListPath) } else { @() })
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
    $sampleListPath = $null
    try {
        if ($SampleFraction -lt 1.0) {
            if ($SampleSeed -eq 0) { $SampleSeed = Get-Random -Minimum 1 -Maximum ([int]::MaxValue) }
            $selectedSpecs = @(New-CdRegressionSampleList -RepoRoot $script:RepoRoot `
                    -Fraction $SampleFraction -Seed $SampleSeed)
            $sampleListPath = Join-Path $script:RepoRoot "temp\cd_regression_sample_$PID.txt"
            [IO.Directory]::CreateDirectory((Split-Path $sampleListPath -Parent)) | Out-Null
            [IO.File]::WriteAllLines($sampleListPath, $selectedSpecs, [Text.UTF8Encoding]::new($false))
            Write-Host ("CD sample: {0}/{1} specs ({2:P1}), seed {3}" -f $selectedSpecs.Count,
                @(Get-ChildItem (Join-Path $script:RepoRoot 'game_data') -Recurse -Filter '*_regression.json5' -File).Count,
                $SampleFraction, $SampleSeed)
        }
        $stages = @(Get-CdRegressionStages -RepoRoot $script:RepoRoot -NoForce:$NoForce `
                -RefreshOracle:$RefreshOracle -SkipLaunch:$SkipLaunch -SpecListPath $sampleListPath)
        Invoke-CdRegressionStages -Stages $stages
    } catch {
        Write-Host ''
        Write-Host "FAILED: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    } finally {
        if ($sampleListPath) { Remove-Item -LiteralPath $sampleListPath -Force -ErrorAction SilentlyContinue }
    }

    Write-Host ''
    Write-Host 'All CD regression stages passed' -ForegroundColor Green
}
