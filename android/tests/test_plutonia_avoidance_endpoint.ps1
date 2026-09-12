#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_plutonia_avoidance_endpoint/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson plutonia.json `
    -Level 28 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Plutonia simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Shoot switch trigger 1|Shoot switch trigger 2|blue key|Shoot switch trigger 5|red key|Reactor|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Plutonia L28 did not complete its route to the intended endpoint'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic Plutonia L28 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/plutonia.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 28)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS Plutonia L28: repeated full-route completion to the intended endpoint, $($result.frames) frames"
