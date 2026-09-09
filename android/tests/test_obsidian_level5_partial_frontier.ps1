#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_obsidian_level5_partial_frontier/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson Obsidian.json -Level 5 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Obsidian level 5 simulation runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Obsidian.json_0_5_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$metadata = Get-Content (Join-Path $repoRoot 'game_data/mission_files/Obsidian.json') -Raw | ConvertFrom-Json
$planned = @($metadata[0].levels | Where-Object level_num -eq 5)[0]
$expected = @($planned.route_steps | Where-Object kind -ne 'start').label -join '|'
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $expected) {
        throw "Obsidian level 5 did not complete its planned objectives: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Obsidian level 5 simulation is not deterministic' }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch '(?s)goal step=9.*semantic=122 navigation=49 keyed=1.*semantic=122 navigation=122 keyed=0.*complete step=9.*label=blue key') {
        throw 'The reached partial path must refresh its closed-door frontier before continuing to the blue key'
    }
}
$normalized = Get-Content (Join-Path $output 'results/Obsidian.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 5)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Obsidian level 5 physical objectives disagree with metadata'
}
Write-Host "PASS Obsidian level 5 partial frontier: $($result.frames) frames, deterministic, metadata agrees"
