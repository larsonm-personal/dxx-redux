#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_tew_hidden_door_approach/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson TEW.json `
    -Level -3 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'TEW simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Open hidden door|Open hidden door|Shoot switch trigger 1|Open hidden door|Shoot switch trigger 2|Open hidden door|Shoot switch trigger 9|Open hidden door|Pass through trigger 5|Open hidden door|Open hidden door|Reactor|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'TEW secret -3 did not complete its hidden-door crossings and route'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic TEW secret -3 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/TEW.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq -3)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS TEW secret -3: repeated hidden-door crossings and complete route, $($result.frames) frames"
