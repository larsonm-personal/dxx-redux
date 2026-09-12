#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_castaway_alternative_exit/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson castaway_redux.json `
    -Level -1 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'castaway_redux simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Fly-through trigger 23|Fly-through trigger 5|Shoot switch trigger 16|Shoot switch trigger 10|Fly-through trigger 13|Destroy blastable wall|Fly-through trigger 9|Shoot switch trigger 8|Shoot switch trigger 3|Shoot switch trigger 2|Reactor|Shoot switch trigger 1|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'castaway_redux secret -1 did not complete its route through the alternative normal exit'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ("logs/" + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'incidental trigger=18' -or $log -notmatch 'goal step=10 kind=5 .*wall=8 trigger=0') {
        throw 'Expected real exit closure followed by selection of the alternative normal exit'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic castaway_redux secret -1 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/castaway_redux.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq -1)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS castaway_redux secret -1: repeated full-route completion through the alternative normal exit, $($result.frames) frames"
