#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_counterstrike_keyed_access_recovery/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson Counterstrike.json `
    -Level 16 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Counterstrike simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'blue key|Shoot switch trigger 3|gold key|Fly-through trigger 10|Fly-through trigger 11|Fly-through trigger 12|red key|Fly-through trigger 16|Fly-through trigger 31|Shoot switch trigger 22|Fly-through trigger 13|Fly-through trigger 15|Fly-through trigger 14|Boss robot|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Counterstrike L16 did not complete the original route and its access recovery'
    }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch 'ROUTE-CONFIRM physical_target semantic=28 navigation=327 .* frontier_keyed=1' -or
        $log -notmatch 'ROUTE-CONFIRM flare wall=25 seg=327 side=5' -or
        $log -notmatch 'ROUTE-CONFIRM complete step=8 .* label=Fly-through trigger 13') {
        throw 'The access route must stop at the owned-key door, open it physically, and cross the return trigger'
    }
    $recoveries = @($result.objectives | Where-Object { $null -ne $_.access_for_route_step })
    if (($recoveries.trigger -join '|') -cne '16|31|22|13|15|14' -or @($recoveries | Where-Object access_for_route_step -ne 8).Count -ne 0) {
        throw 'The distinct access triggers must preserve the original boss objective'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic Counterstrike L16 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/Counterstrike.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 16)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS Counterstrike L16: access triggers beyond owned-key doors, $($result.frames) frames"
