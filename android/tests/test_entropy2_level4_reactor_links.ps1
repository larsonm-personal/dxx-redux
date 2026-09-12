#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_entropy2_level4_reactor_links/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -Mode Headless -MissionJson Entropy2.json `
    -Level 4 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Entropy2 simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Entropy2.json_0_4_e2v-4.rl2_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$sequence = 'Shoot switch trigger 0|Destroy blastable wall|blue key|Shoot switch trigger 2|Reactor|Pass through trigger 12|Exit'
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw "Incomplete reactor escape: $($result.status)"
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic reactor escape' }
    $log = Get-Content -LiteralPath (Join-Path $output "logs/$($file.BaseName).log") -Raw
    if ($log -notmatch '(?s)complete step=5.*label=Reactor.*complete step=6.*label=Pass through trigger 12.*complete step=7.*label=Exit') {
        throw 'The engine must activate the exit-path trigger after destroying the reactor'
    }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/Entropy2.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 4)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') { throw 'Physical objectives disagree with metadata' }
Write-Host "PASS Entropy2 L4: $($result.frames) frames, deterministic reactor escape, metadata agrees"
