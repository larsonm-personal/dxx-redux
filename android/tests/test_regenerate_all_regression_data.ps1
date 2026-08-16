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

$unknownArgumentRejected = $false
try {
    . $runnerPath -Force
} catch {
    $unknownArgumentRejected = $_.FullyQualifiedErrorId -like 'NamedParameterNotFound*'
}
Assert-True $unknownArgumentRejected 'Master regeneration should reject unsupported arguments before running stages'

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
Assert-True (@(Get-RegressionDataStages -RepoRoot $repoRoot -Category Cd).Count -eq 1) `
    'CD-only selection should contain one stage'
Assert-True ((Get-RegressionDataStages -RepoRoot $repoRoot -Category Fingerprints)[0].Key -eq 'Fingerprints') `
    'Fingerprint-only selection should select the fingerprint stage'
Assert-True ((Get-RegressionDataStages -RepoRoot $repoRoot -Category Metadata)[0].Key -eq 'Metadata') `
    'Metadata-only selection should select the metadata stage'

foreach ($templateName in @(
        'test_mission_zip_batch_import_metadata.json5',
        'test_mission_zip_batch_import_metadata_launch.json5'
    )) {
    $templatePath = Join-Path $repoRoot "android\game_scripts\$templateName"
    $template = Get-Content -LiteralPath $templatePath -Raw
    $routeCacheClear = $template.IndexOf('"command": "clear_route_metadata_cache"')
    $resultCacheClear = $template.IndexOf('"command": "clear_level_metadata_result_cache"')
    $metadataAnalysis = $template.IndexOf('"action": "analyze_level_metadata_all"')
    Assert-True ($routeCacheClear -ge 0 -and $routeCacheClear -lt $metadataAnalysis) `
        "$templateName should clear the native route cache before metadata analysis"
    Assert-True ($resultCacheClear -ge 0 -and $resultCacheClear -lt $metadataAnalysis) `
        "$templateName should clear the full-result cache before metadata analysis"
}

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

    $reportDir = Join-Path $tempRoot 'reports'
    $results = @(Invoke-RegressionDataStages -Stages $testStages -ReportDir $reportDir -Category All -RecordTiming)
    Assert-True ($results.Count -eq 3) `
        "Master regeneration should return every stage result, got $($results.Count): $($results.GetType().FullName)"
    Assert-True (($results.Status -join ',') -eq 'PASS,FAIL,PASS') `
        'Master regeneration should preserve each stage status'
    $executed = @(Get-Content -LiteralPath $logPath)
    Assert-True (($executed -join ',') -eq 'first,second,third') `
        'Master regeneration should continue after a stage failure'

    $runDir = @(Get-ChildItem -LiteralPath $reportDir -Directory -Filter 'run_*')
    Assert-True ($runDir.Count -eq 1) 'Full regeneration should create one durable run directory'
    $summaryPath = Join-Path $runDir[0].FullName 'summary.json'
    Assert-True (Test-Path -LiteralPath $summaryPath -PathType Leaf) `
        'Full regeneration should save a machine-readable summary'
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-True ($summary.status -eq 'fail' -and $summary.stages.Count -eq 3) `
        'Durable summary should contain the complete failed run'
    Assert-True (@(Get-ChildItem -LiteralPath $runDir[0].FullName -Filter '*.log').Count -eq 3) `
        'Full regeneration should save one log per stage'
    Assert-True (@(Get-ChildItem -LiteralPath $reportDir -File -Filter 'report_*.md').Count -eq 1) `
        'Complete full regeneration should save timing history even when a stage fails'

    $partialReportDir = Join-Path $tempRoot 'partial_reports'
    $partial = @($testStages | Select-Object -First 1)
    $partialResults = @(Invoke-RegressionDataStages -Stages $partial -ReportDir $partialReportDir -Category Cd)
    Assert-True ($partialResults.Count -eq 1 -and $partialResults[0].Status -eq 'PASS') `
        'Partial regeneration should run its selected stage'
    Assert-True (@(Get-ChildItem -LiteralPath $partialReportDir -File -Filter 'report_*.md').Count -eq 0) `
        'Partial regeneration should not write timing history'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'Master regression data regeneration tests passed' -ForegroundColor Green
exit 0
