#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_mission_metadata_host.ps1'
$output = Join-Path $repoRoot "android/temp/test_vertigo_metadata_checkpoints/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

# The CD runner retains its normal 30-second deadline with checkpoints enabled
& (Get-Process -Id $PID).Path -NoProfile -File $runner -NoRegressionCopy `
    -CdSourcesOnly -CdSourceIds descent-ii-vertigo-usa -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Vertigo CD metadata regeneration failed' }

$summary = @(Get-Content (Join-Path $output 'summary.json') -Raw | ConvertFrom-Json)
if ($summary.Count -ne 1 -or $summary[0].status -ne 'passed') {
    throw 'Vertigo CD source did not pass'
}
$raw = Get-Content (Join-Path $output 'raw/cd_descent-ii-vertigo-usa.d2x.metadata.json') -Raw | ConvertFrom-Json
if ($raw.status -ne 'ok' -or $raw.levels.Count -ne 23 -or @($raw.levels | Where-Object status -ne 'ok').Count) {
    throw 'Vertigo metadata did not include all 23 valid levels'
}
$checkpoint = Get-Content (Join-Path $output 'logs/cd_descent-ii-vertigo-usa.d2x.log.checkpoint.json') -Raw | ConvertFrom-Json
if ($checkpoint.stage -ne 'done' -or $checkpoint.detail -ne 'ok' -or $checkpoint.request_id -ne $raw.request_id) {
    throw 'Vertigo final checkpoint was lost or belongs to another request'
}
Write-Host 'PASS Vertigo metadata: all 23 levels within the normal deadline, final checkpoint retained'
