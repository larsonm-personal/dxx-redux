#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_obsidian_level13_remote_door/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless `
    -MissionJson Obsidian.json -Level 13 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Obsidian level 13 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Obsidian.json_0_13_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = @(
    'Shoot switch trigger 12', 'Open hidden door', 'Shoot switch trigger 13',
    'blue key', 'Shoot switch trigger 7', 'Pass through trigger 3',
    'Shoot switch trigger 2', 'Shoot switch trigger 6', 'Shoot switch trigger 8',
    'red key', 'Boss robot', 'Exit'
)
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Obsidian level 13 did not reach the exit: $($result.status), $($result.problem)"
    }
    if (($result.objectives.label -join '|') -cne ($expected -join '|')) {
        throw 'Obsidian level 13 did not open the remote prerequisite door and complete its route in order'
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch 'flare wall=55 seg=343 side=0 actor_seg=342') {
        throw 'Obsidian level 13 did not shoot the hidden door through the grate'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Obsidian level 13 simulation is not deterministic' }
}
$normalized = Get-Content (Join-Path $output 'results/Obsidian.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 13)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Obsidian level 13 physical objectives disagree with its metadata route'
}
Write-Host "PASS Obsidian level 13 door route: $($result.frames) frames, deterministic, metadata agrees"
