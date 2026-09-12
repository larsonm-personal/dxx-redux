#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_maximum_restoring_wall/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson descent_maximum_fixed.json `
    -Level 23 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Maximum simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'blue key|Shoot switch trigger 1|Fly-through trigger 2|gold key|red key|Reactor|Fly-through trigger 13|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Maximum L23 did not complete its original route'
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch 'ROUTE-CONFIRM incidental trigger=15 seg=43 side=4 actor_seg=47' -or
        $log -notmatch 'ROUTE-CONFIRM incidental trigger=13 seg=47 side=5 actor_seg=43' -or
        $log -notmatch 'ROUTE-CONFIRM flare wall=9 seg=20 side=5' -or
        $log -notmatch 'ROUTE-CONFIRM flare wall=14 seg=19 side=4') {
        throw 'The proof must execute the wall closure and exit opener, then open the exit route doors'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic Maximum L23 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/descent_maximum_fixed.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 23)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS Maximum L23: route avoids a restoring wall, $($result.frames) frames"
