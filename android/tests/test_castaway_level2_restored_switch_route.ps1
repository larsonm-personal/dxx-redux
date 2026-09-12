#!/usr/bin/env pwsh

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot `
    "android\temp\test_castaway_level2_restored_switch_$([guid]::NewGuid().ToString('N'))"

# Build the Windows D2 engine first; this test never overwrites corpus results
& $runner -Mode Headless -MissionJson castaway_redux.json -Level 2 -Repeat 2 `
    -NoBuild -OutputRoot $output

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'castaway_redux.json_0_2_*_run_*.json' -File |
        Where-Object { $_.Name -match '_run_\d+\.json$' } | Sort-Object Name)
if ($results.Count -ne 2) {
    throw "Expected two Castaway level 2 results, found $($results.Count)"
}
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $labels = @($result.objectives.label)
    $last = -1
    foreach ($required in @('blue key', 'gold key', 'red key', 'Fly-through trigger 19',
            'Shoot switch trigger 24', 'Fly-through trigger 16',
            'Shoot switch trigger 21', 'Shoot switch trigger 20', 'Reactor', 'Exit')) {
        $index = [Array]::IndexOf($labels, $required)
        if ($index -le $last) {
            throw "Castaway level 2 missing/out-of-order objective: $required (status=$($result.status), problem=$($result.problem))"
        }
        $last = $index
    }
    if ($result.status -ne 'confirmed') {
        throw "Castaway level 2 route was not confirmed: $($result.problem)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) {
        $first = $signature
    } elseif ($signature -ne $first) {
        throw 'Castaway level 2 route simulation is not deterministic'
    }
}
Write-Host "PASS Castaway level 2 restored-switch route: $($result.frames) frames, deterministic"
