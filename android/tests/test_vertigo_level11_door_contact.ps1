#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_vertigo_level11_door_contact/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
$mission = 'CD - Descent II - The Vertigo Series (USA).json'
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson $mission -Level 11 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Vertigo level 11 simulation runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter '*_0_11_d2xlvl11.rl2_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = 'blue key|gold key|Shoot switch trigger 8|red key|Shoot switch trigger 6|Reactor|Exit'
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $expected) {
        throw "Vertigo level 11 did not complete its door, switch and exit sequence: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Vertigo level 11 simulation is not deterministic' }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch '(?s)flare wall=92 seg=348 side=4.*complete step=3.*label=Shoot switch trigger 8.*complete step=7.*label=Exit') {
        throw 'The bot must open the yellow-key door and continue to the switch and exit'
    }
}
$normalized = Get-Content (Join-Path $output 'results/CD - Descent II - The Vertigo Series (USA).simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 11)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Vertigo level 11 physical objectives disagree with metadata'
}
Write-Host "PASS Vertigo level 11 door contact: $($result.frames) frames, deterministic, metadata agrees"
