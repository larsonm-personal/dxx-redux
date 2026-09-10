#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
# Funnel recovery must preserve ordinary D1 and D2 collision behavior
# These recorded inputs caught the earlier global swept-edge correction
& (Join-Path $PSScriptRoot 'run_input_demo_regressions.ps1') -RunMode headless -Mode accelerated `
    -DemoFileName @('d1_descent_level16_20260618_201843.dximdemo', 'd2_descent2_level9_20260511_215654.dximdemo')
if ($LASTEXITCODE -ne 0) { throw 'Funnel recovery changed base-game demo results' }
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_plutonia_narrow_funnel/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson plutonia.json `
    -Level 3 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Plutonia simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Open hidden door|Shoot switch trigger 2|blue key|gold key|Shoot switch trigger 6|Fly-through trigger 9|red key|Reactor|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Plutonia L3 did not complete its route through the narrow funnel'
    }
    $log = Get-Content -LiteralPath (Join-Path $output ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'ROUTE-CONFIRM recover alternate ') {
        throw 'Expected native-physics alternate-path recovery at the blocked funnel portal'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic Plutonia L3 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/plutonia.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 3)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS Plutonia L3: repeated full-route completion with alternate-path funnel recovery, $($result.frames) frames"
