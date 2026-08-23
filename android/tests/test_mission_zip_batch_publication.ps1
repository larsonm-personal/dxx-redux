#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$batchPath = Join-Path $repoRoot "android\helpers\run_mission_zip_batch.ps1"
$source = [System.IO.File]::ReadAllText($batchPath)

if ($source -match 'Write-MissionZipFailureJson -Path \$regressionJsonPath') {
    throw "Failed mission ZIP runs can overwrite checked-in regression metadata"
}
if ($source -notmatch '(?s)\$runId = \[Guid\]::NewGuid.*?"--es", "run_id", \$runId.*?Watch-AutomationResult.*?-ExpectedRunId \$runId') {
    throw "Mission ZIP automation is not correlated by run ID"
}
if ($source -notmatch '(?s)if \(\$record\["status"\] -eq "passed"\).*?\$metadataSaved.*?Write-Utf8NoBomTextAtomically -Path \$regressionJsonPath') {
    throw "Successful validated metadata is no longer the only checked-in publication path"
}

Write-Host "Mission ZIP batch publication tests passed"
