#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_obsidian_level11_door/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless `
    -MissionJson Obsidian.json -Level 11 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Obsidian level 11 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Obsidian.json_0_11_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = @(
    'Shoot switch trigger 5', 'Shoot switch trigger 6', 'blue key',
    'Fly-through trigger 10', 'Shoot switch trigger 7', 'Shoot switch trigger 8',
    'Shoot switch trigger 9', 'gold key', 'Shoot switch trigger 17',
    'Shoot switch trigger 16', 'Pass through trigger 14', 'Shoot switch trigger 15',
    'red key', 'Shoot switch trigger 2', 'Shoot switch trigger 3',
    'Shoot switch trigger 4', 'Shoot switch trigger 1', 'Reactor', 'Exit'
)
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Obsidian level 11 did not reach the exit: $($result.status), $($result.problem)"
    }
    if (($result.objectives.label -join '|') -cne ($expected -join '|')) {
        throw 'Obsidian level 11 did not unlock the prerequisite door and complete its route in order'
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -match 'replan closed trigger door wall=80') {
        throw 'Obsidian level 11 repeated the locked-door replan loop'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Obsidian level 11 simulation is not deterministic' }
}
$normalized = Get-Content (Join-Path $output 'results/Obsidian.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 11)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Obsidian level 11 physical objectives disagree with its metadata route'
}
Write-Host "PASS Obsidian level 11 door route: $($result.frames) frames, deterministic, metadata agrees"
