#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_plutonia_level11_boss_grate/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson plutonia.json -Level 11 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Plutonia level 11 simulation runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'plutonia.json_0_11_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = 'blue key|red key|Pass through trigger 6|Shoot switch trigger 7|Boss robot|Exit'
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $expected) {
        throw "Plutonia level 11 did not complete its arena dependencies and exit: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Plutonia level 11 simulation is not deterministic' }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch '(?s)complete step=3.*label=Pass through trigger 6.*verified switch shot.*wall=63.*complete step=4.*label=Shoot switch trigger 7.*goal step=5.*goal_seg=726 step_seg=532 path_terminal=726.*complete step=5.*label=Boss robot') {
        throw 'The boss must be fought from outside the grates after physically opening the arena'
    }
}
$normalized = Get-Content (Join-Path $output 'results/plutonia.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object level_num -eq 11)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Plutonia level 11 physical objectives disagree with metadata'
}
Write-Host "PASS Plutonia level 11 boss grate: $($result.frames) frames, deterministic, metadata agrees"
