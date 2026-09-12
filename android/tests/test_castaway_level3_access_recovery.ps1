#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_castaway_level3_access_recovery/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson castaway_redux.json `
    -Level 3 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Castaway simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'blue key|gold key|red key|Fly-through trigger 5|Shoot switch trigger 6|Shoot switch trigger 19|Shoot switch trigger 7|Shoot switch trigger 2|Reactor|Shoot switch trigger 0|Fly-through trigger 1|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Castaway L3 did not complete its crossed-trigger route'
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch 'ROUTE-CONFIRM incidental trigger=4 seg=604 side=0 actor_seg=599' -or
        $log -notmatch 'ROUTE-CONFIRM verified switch shot .* wall=102') {
        throw 'The test must close the old path and hit the remote recovery switch physically'
    }
    $recoveries = @($result.objectives | Where-Object { $null -ne $_.access_for_route_step })
    if (($recoveries.trigger -join '|') -cne '5|6|19|7|0|1') {
        throw 'Each distinct access trigger must have its own completion record'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic Castaway L3 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/castaway_redux.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 3)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS Castaway L3: repeated access recoveries after the return path closes, $($result.frames) frames"
