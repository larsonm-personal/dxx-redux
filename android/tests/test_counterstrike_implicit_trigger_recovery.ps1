#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_counterstrike_implicit_trigger_recovery/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson Counterstrike.json `
    -Level 5 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Counterstrike simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'blue key|Fly-through trigger 9|Shoot switch trigger 11|red key|Shoot switch trigger 15|Shoot switch trigger 15|Fly-through trigger 6|Reactor|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Counterstrike L5 did not complete the original route and its access recovery'
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch 'ROUTE-CONFIRM implicit step=5 .* label=Shoot switch trigger 15' -or
        $log -notmatch 'ROUTE-CONFIRM replan crossed path closure trigger=14' -or
        $log -notmatch 'ROUTE-CONFIRM verified switch shot .* wall=142') {
        throw 'The test must record the initial open state, replan after closure, and physically hit the recovery switch'
    }
    $recoveries = @($result.objectives | Where-Object { $null -ne $_.access_for_route_step })
    if ($recoveries.Count -ne 1 -or $recoveries[0].trigger -ne 15 -or $recoveries[0].access_for_route_step -ne 5) {
        throw 'The opening switch must be recorded as access work for the pending crossing'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic Counterstrike L5 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/Counterstrike.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 5)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS Counterstrike L5: implicit trigger completion recovery, $($result.frames) frames"
