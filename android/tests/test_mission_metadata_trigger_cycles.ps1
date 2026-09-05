#!/usr/bin/env pwsh

param([switch]$Build)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_mission_metadata_host.ps1'
$outputRoot = Join-Path $repoRoot "android\temp\metadata_trigger_cycles_$([guid]::NewGuid().ToString('N'))"

# These archives exercised mutually dependent switch preparations in both engines
& $runner -ArchiveNames 'KAK.ZIP', 'levigen.zip', 'D1-levelpack.7z' `
    -NoRegressionCopy -NoBuild:(-not $Build) -OutputRoot $outputRoot
if ($LASTEXITCODE -ne 0) { throw "Trigger-cycle metadata regeneration failed with exit code $LASTEXITCODE" }
$records = @(Get-Content -LiteralPath (Join-Path $outputRoot 'summary.jsonl') |
        ForEach-Object { $_ | ConvertFrom-Json })
if ($records.Count -ne 3 -or @($records | Where-Object status -ne 'passed').Count -ne 0) {
    throw 'Expected all three trigger-cycle archives to produce metadata'
}
Write-Host 'Mission metadata trigger-cycle integration passed'
