#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_obsidian_level12_switch_approach/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless `
    -MissionJson Obsidian.json -Level 12 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Obsidian level 12 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Obsidian.json_0_12_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = @(
    'Shoot switch trigger 3',
    'blue key',
    'Shoot switch trigger 2',
    'Shoot switch trigger 4',
    'gold key',
    'Shoot switch trigger 1',
    'Fly-through trigger 9',
    'Shoot switch trigger 10',
    'Fly-through trigger 18',
    'Shoot switch trigger 12',
    'Shoot switch trigger 7',
    'Shoot switch trigger 6',
    'red key',
    'Boss robot',
    'Exit'
)
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Obsidian level 12 did not reach the exit: $($result.status), $($result.problem)"
    }
    if (($result.objectives.label -join '|') -cne ($expected -join '|')) {
        throw 'Obsidian level 12 did not activate the shallow-segment switch and complete its route in order'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Obsidian level 12 simulation is not deterministic' }
}
$normalized = Get-Content (Join-Path $output 'results/Obsidian.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 12)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Obsidian level 12 physical objectives disagree with its metadata route'
}
Write-Host "PASS Obsidian level 12 switch approach: $($result.frames) frames, deterministic, metadata agrees"
