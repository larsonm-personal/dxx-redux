#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$missionPath = Join-Path $repoRoot 'game_data\mission_files\Obsidian.json'
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot `
    "android\temp\test_obsidian_level4_closed_trigger_source_$([guid]::NewGuid().ToString('N'))"

$mission = Get-Content -LiteralPath $missionPath -Raw | ConvertFrom-Json
$level = @($mission.levels | Where-Object { $_.level_num -eq 4 })
if ($level.Count -ne 1) {
    throw "Expected exactly one Obsidian level 4 record, found $($level.Count)"
}
$labels = @($level[0].route_steps.label)
$trigger6 = [Array]::IndexOf($labels, 'Shoot switch trigger 6')
$trigger22 = [Array]::IndexOf($labels, 'Fly-through trigger 22')
if ($trigger6 -lt 0 -or $trigger22 -lt 0 -or $trigger6 -ge $trigger22) {
    throw "Obsidian level 4 does not open wall 79 before crossing trigger 22: $($labels -join ' | ')"
}

& $runner -Mode Headless -MissionJson Obsidian.json -Level 4 -Repeat 2 `
    -NoBuild -OutputRoot $output

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Obsidian.json_0_4_*_run_*.json' | Sort-Object Name)
if ($results.Count -ne 2) {
    throw "Expected two Obsidian level 4 results, found $($results.Count)"
}
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Obsidian level 4 route was not confirmed: status=$($result.status) problem=$($result.problem)"
    }
    if (@($result.objectives.label) -notcontains 'Shoot switch trigger 6' -or
        @($result.objectives.label) -notcontains 'Fly-through trigger 22' -or
        @($result.objectives.label)[-1] -ne 'Exit') {
        throw "Obsidian level 4 did not execute the corrected prerequisite chain: $($file.Name)"
    }
    $signature = "$($result.status)|$($result.frames)|$($result.rng_end.simulation.state)|$($result.rng_end.simulation.calls)"
    if ($null -eq $first) {
        $first = $signature
    } elseif ($signature -ne $first) {
        throw 'Obsidian level 4 route simulation is not deterministic'
    }
}

Write-Host "PASS Obsidian level 4 closed trigger source route: $first"
