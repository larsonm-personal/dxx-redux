#!/usr/bin/env pwsh

param([switch]$Build)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_mission_metadata_host.ps1'
$outputParent = Join-Path $repoRoot 'android\temp\mission_zip_host_metadata'

& $runner -NoRegressionCopy -NoBuild:(-not $Build) -ArchiveName 'Obsidian.zip'
if ($LASTEXITCODE -ne 0) { throw "Windows Obsidian metadata regeneration failed with exit code $LASTEXITCODE" }

$output = Get-ChildItem -LiteralPath $outputParent -Directory |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1
if (-not $output) { throw 'Windows metadata regeneration produced no output directory' }

$rawPath = Join-Path $output.FullName 'raw\obsidian.obsidian.metadata.json'
$projectedPath = Join-Path $output.FullName 'metadata\Obsidian.json'
$raw = Get-Content -LiteralPath $rawPath -Raw | ConvertFrom-Json
$projected = @(Get-Content -LiteralPath $projectedPath -Raw | ConvertFrom-Json)[0]
$missing = @($raw.levels | Where-Object {
        $null -eq $_.PSObject.Properties['route_required_key_mask'] -or
        $null -eq $_.PSObject.Properties['route_completing_key_mask_set']
    })
if ($missing.Count) { throw "$($missing.Count) native Obsidian levels omitted route key-mask fields" }

$level1 = @($projected.levels | Where-Object { $_.level_num -eq 1 })[0]
if ($level1.route_required_key_mask -ne 3 -or $level1.route_completing_key_mask_set -ne 8) {
    throw "Obsidian level 1 masks are $($level1.route_required_key_mask)/$($level1.route_completing_key_mask_set), expected 3/8"
}
if (@($projected.track_names).Count -ne 14) {
    throw "Obsidian projected $(@($projected.track_names).Count) MIDI tracks, expected 14"
}

Write-Host 'Windows mission metadata route-mask integration passed' -ForegroundColor Green
