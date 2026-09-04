#!/usr/bin/env pwsh

[CmdletBinding()]
param([switch]$Build)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$outputRoot = Join-Path $repoRoot `
    "android\temp\obsidian_level3_blastable_wall_test_$([guid]::NewGuid().ToString('N'))"

& $runner -Mode Headless -MissionJson Obsidian.json -Level 3 -Repeat 2 `
    -NoBuild:(-not $Build) -OutputRoot $outputRoot

$results = @(Get-ChildItem (Join-Path $outputRoot 'results') -File `
        -Filter '*_run_*.json' | Sort-Object Name)
if ($results.Count -ne 2) {
    throw "Expected two Obsidian level 3 results, found $($results.Count)"
}

foreach ($file in $results) {
    $result = Get-Content -Raw $file.FullName | ConvertFrom-Json
    if (@($result.objectives).Count -eq 0) {
        throw "Obsidian level 3 made no progress beyond its initial blastable wall: $($file.Name)"
    }
    if (-not @($result.objectives | Where-Object {
                $_.label -eq 'Fly-through trigger 7'
            }).Count) {
        throw "Obsidian level 3 did not route around the closed floor to trigger 7: $($file.Name)"
    }
    if (@($result.objectives | Where-Object {
                $_.label -eq 'Destroy blastable wall'
            }).Count -lt 3) {
        throw "Obsidian level 3 did not reach the next objective after the closed-floor detour: $($file.Name)"
    }
    if (-not @($result.objectives | Where-Object {
                $_.label -eq 'Fly-through trigger 8'
            }).Count) {
        throw "Obsidian level 3 reversed its path before fly-through trigger 8: $($file.Name)"
    }
}

if ((Get-FileHash $results[0].FullName -Algorithm SHA256).Hash -ne
    (Get-FileHash $results[1].FullName -Algorithm SHA256).Hash) {
    throw 'Obsidian level 3 blastable-wall simulation is not deterministic'
}

Write-Host "Obsidian level 3 blastable wall passed: status=$($result.status) frames=$($result.frames)"
