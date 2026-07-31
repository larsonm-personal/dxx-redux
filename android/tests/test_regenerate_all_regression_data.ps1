#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$runnerPath = Join-Path $repoRoot 'android\regenerate_all_regression_data.ps1'
. $runnerPath

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

$stages = @(Get-RegressionDataStages -RepoRoot $repoRoot)
Assert-True ($stages.Count -eq 3) 'Master regeneration should contain exactly three wrapper stages'
Assert-True ($stages[0].Script -eq (Join-Path $repoRoot 'game_data\run_all_cd_regressions.ps1')) `
    'The first stage should use the canonical CD regression wrapper'
Assert-True (($stages[0].Arguments -join ',') -eq '-RefreshOracle') `
    'The CD stage should explicitly refresh its regression oracle'
Assert-True ($stages[1].Script -eq (Join-Path $repoRoot 'game_data\update_all_fingerprints.ps1')) `
    'The second stage should use the complete fingerprint wrapper'
Assert-True (($stages[1].Arguments -join ',') -eq '-Force') `
    'The fingerprint stage should force a complete refresh'
Assert-True ($stages[2].Script -eq (Join-Path $repoRoot 'android\helpers\regenerate_all_mission_metadata.ps1')) `
    'The third stage should use the canonical Android mission metadata wrapper'

$tempRoot = Join-Path $repoRoot 'android\temp\regression_data_runner_test'
if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

try {
    $logPath = Join-Path $tempRoot 'stages.log'
    $testStages = @()
    foreach ($definition in @(
            @{ Name = 'first'; ExitCode = 0 },
            @{ Name = 'second'; ExitCode = 7 },
            @{ Name = 'third'; ExitCode = 0 }
        )) {
        $scriptPath = Join-Path $tempRoot "$($definition.Name).ps1"
        $content = @"
param([string]`$LogPath)
Add-Content -LiteralPath `$LogPath -Value '$($definition.Name)'
exit $($definition.ExitCode)
"@
        [System.IO.File]::WriteAllText($scriptPath, $content, [System.Text.UTF8Encoding]::new($false))
        $testStages += [pscustomobject]@{
            Name = $definition.Name
            Script = $scriptPath
            Arguments = @($logPath)
        }
    }

    $failed = $false
    try {
        Invoke-RegressionDataStages -Stages $testStages
    } catch {
        $failed = $_.Exception.Message -match "stage 'second' failed with exit code 7"
    }
    Assert-True $failed 'Master regeneration should report the failing stage and exit code'
    $executed = @(Get-Content -LiteralPath $logPath)
    Assert-True (($executed -join ',') -eq 'first,second') `
        'Master regeneration should stop before the stage after a failure'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'Master regression data regeneration tests passed' -ForegroundColor Green
exit 0
