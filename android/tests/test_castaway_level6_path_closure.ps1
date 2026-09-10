#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_castaway_level6_path_closure/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson castaway_redux.json `
    -Level 6 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Castaway simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Shoot switch trigger 3|blue key|Fly-through trigger 1|Shoot switch trigger 2|gold key|Shoot switch trigger 13|Fly-through trigger 18|Shoot switch trigger 19|Fly-through trigger 38|red key|Fly-through trigger 9|Shoot switch trigger 10|Fly-through trigger 12|Shoot switch trigger 11|Shoot switch trigger 21|Boss robot|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Castaway L6 did not complete the original route and its access recovery'
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch 'ROUTE-CONFIRM incidental trigger=28 seg=672 side=0' -or
        $log -notmatch 'ROUTE-CONFIRM replan crossed path closure trigger=28' -or
        $log -notmatch 'ROUTE-CONFIRM verified switch shot .* wall=165') {
        throw 'The test must execute the closing trigger, replan, and physically hit the recovery switch'
    }
    $recoveries = @($result.objectives | Where-Object { $null -ne $_.access_for_route_step })
    if ($recoveries.Count -ne 1 -or $recoveries[0].trigger -ne 13 -or $recoveries[0].access_for_route_step -ne 6) {
        throw 'The opening switch must be recorded as access work for the pending crossing'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic Castaway L6 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/castaway_redux.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 6)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS Castaway L6: crossed path closure recovery, $($result.frames) frames"
