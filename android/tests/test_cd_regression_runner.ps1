#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$runnerPath = Join-Path $repoRoot 'game_data\run_all_cd_regressions.ps1'
. $runnerPath

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

$defaultStages = @(Get-CdRegressionStages -RepoRoot $repoRoot)
Assert-True ($defaultStages.Count -eq 3) 'Default workflow should contain three stages'
Assert-True ($defaultStages[0].Arguments -contains '-Force') 'Extraction should be forced by default'
Assert-True ($defaultStages.Name -notcontains 'Refresh regression specs') 'Default workflow must not rewrite its oracle'
Assert-True ($defaultStages[2].Arguments -contains '-All') 'Regression suite should run all specs'
Assert-True ($defaultStages[2].Arguments -contains '-SkipLaunch') 'Regression suite should default to file-only mode'

$refreshStages = @(Get-CdRegressionStages -RepoRoot $repoRoot -RefreshOracle)
Assert-True ($refreshStages.Count -eq 4) 'Oracle refresh workflow should contain four stages'
Assert-True ($refreshStages[1].Name -eq 'Refresh regression specs') 'Oracle refresh stage should be explicit'
Assert-True ($refreshStages[1].Arguments -contains '-Force') 'Explicit oracle refresh should regenerate all specs'

$tempRoot = Join-Path $repoRoot 'android\temp\cd_regression_runner_test'
if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

try {
    $logPath = Join-Path $tempRoot 'stages.log'
    $scripts = @()
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
        $scripts += [pscustomobject]@{
            Name = $definition.Name
            Script = $scriptPath
            Arguments = @($logPath)
        }
    }

    $failed = $false
    try {
        Invoke-CdRegressionStages -Stages $scripts
    } catch {
        $failed = $_.Exception.Message -match "stage 'second' failed with exit code 7"
    }
    Assert-True $failed 'Runner should report the failing stage and exit code'
    $executed = @(Get-Content -LiteralPath $logPath)
    Assert-True (($executed -join ',') -eq 'first,second') 'Runner should stop before the stage after a failure'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'CD regression runner tests passed' -ForegroundColor Green
exit 0
