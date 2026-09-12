#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_bitesize_crossed_triggers/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson bitesize.json `
    -Level 10 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Bitesize simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Destroy blastable wall|Open hidden door|blue key|Shoot switch trigger 3|gold key|red key|Reactor|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Bitesize L10 did not complete its crossed-trigger route'
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch 'ROUTE-CONFIRM incidental trigger=2 seg=18 side=3 actor_seg=19' -or
        $log -notmatch 'ROUTE-CONFIRM incidental trigger=5 seg=72 side=0 actor_seg=68') {
        throw 'The route must apply crossed door-opening and door-closing triggers'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic Bitesize L10 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/bitesize.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 10)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS Bitesize L10: repeated physical crossings with opening and closing effects, $($result.frames) frames"
