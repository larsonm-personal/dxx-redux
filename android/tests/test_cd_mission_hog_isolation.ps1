#!/usr/bin/env pwsh

param([switch]$Build)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot "android\temp\cd_hog_isolation_$([guid]::NewGuid().ToString('N'))"
& (Join-Path $repoRoot 'android\helpers\regenerate_all_mission_metadata_host.ps1') `
    -CdSourcesOnly -CdSourceIds 'descent-anniversary-usa-extras', 'descent-levels-of-the-world-usa' `
    -NoBuild:(-not $Build) -NoRegressionCopy -OutputRoot $outputRoot
if ($LASTEXITCODE -ne 0) { throw "CD metadata regeneration failed with exit code $LASTEXITCODE" }
$files = @(Get-ChildItem -LiteralPath (Join-Path $outputRoot 'metadata') -Filter '*.json')
if ($files.Count -ne 2) { throw 'Expected both CD collection outputs' }
foreach ($file in $files) {
    $missions = @(Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json)
    if ($missions.Count -eq 0 -or @($missions | Where-Object { $_.status -ne 'ok' -or $_.levels.Count -eq 0 }).Count) {
        throw "CD collection contains failed or empty mission metadata: $($file.Name)"
    }
}
Write-Host 'CD mission HOG isolation tests passed'
