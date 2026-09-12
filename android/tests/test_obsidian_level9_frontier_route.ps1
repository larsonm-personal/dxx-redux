#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_obsidian_level9_frontier/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless `
    -MissionJson Obsidian.json -Level 9 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Obsidian level 9 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Obsidian.json_0_9_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = @(
    'blue key', 'Destroy blastable wall', 'Fly-through trigger 0',
    'Pass through trigger 7', 'Shoot switch trigger 8', 'gold key', 'red key',
    'Fly-through trigger 1', 'Boss robot', 'Exit'
)
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Obsidian level 9 did not reach the exit: $($result.status), $($result.problem)"
    }
    if (($result.objectives.label -join '|') -cne ($expected -join '|')) {
        throw 'Obsidian level 9 did not destroy the prerequisite wall and complete its route in order'
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch 'wall=95 trigger=-1 label=Destroy blastable wall') {
        throw 'Obsidian level 9 did not target the blastable entrance to trigger 0'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Obsidian level 9 simulation is not deterministic' }
}
$normalized = Get-Content (Join-Path $output 'results/Obsidian.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 9)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Obsidian level 9 physical objectives disagree with its metadata route'
}
Write-Host "PASS Obsidian level 9 frontier route: $($result.frames) frames, deterministic, metadata agrees"
