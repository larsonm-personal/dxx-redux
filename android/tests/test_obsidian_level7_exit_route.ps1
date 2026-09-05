#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot `
    "android\temp\test_obsidian_level7_exit_route_$([guid]::NewGuid().ToString('N'))"

# Run after building the Windows D2 engine. Never change checked-in results here.
& $runner -Mode Headless -MissionJson Obsidian.json -Level 7 -Repeat 2 `
    -NoBuild -OutputRoot $output

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Obsidian.json_0_7_*_run_*.json' | Sort-Object Name)
if ($results.Count -ne 2) {
    throw "Expected two Obsidian level 7 results, found $($results.Count)"
}
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Obsidian level 7 route was not confirmed: status=$($result.status) problem=$($result.problem)"
    }
    $labels = @($result.objectives.label)
    foreach ($required in @('blue key', 'gold key', 'red key', 'Reactor')) {
        if ($labels -notcontains $required) {
            throw "Obsidian level 7 omitted objective: $required"
        }
    }
    if ($labels[-1] -ne 'Exit' -or $result.radius.effective -lt
        [Math]::Max($result.radius.player, $result.radius.guidebot)) {
        throw 'Obsidian level 7 must reach the exit using at least the larger actor radius'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) {
        $first = $signature
    } elseif ($signature -ne $first) {
        throw 'Obsidian level 7 route simulation is not deterministic'
    }
}
Write-Host "PASS Obsidian level 7 reactor-to-exit route: $($result.frames) frames, deterministic"
