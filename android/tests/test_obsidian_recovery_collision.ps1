#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_obsidian_recovery_collision/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
& (Get-Process -Id $PID).Path -NoProfile -File $runner -MissionJson Obsidian.json `
    -Level 14 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Obsidian simulation runner failed' }
$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw 'Expected two repeated runs' }
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Fly-through trigger 2|Fly-through trigger 15|Fly-through trigger 19|Destroy robot carrying blue key|Shoot switch trigger 6|Destroy robot carrying gold key|Fly-through trigger 19|Shoot switch trigger 5|Fly-through trigger 19|Shoot switch trigger 3|Boss robot|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Obsidian L14 did not complete its route after the recovery switch approach'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic Obsidian L14 result' }
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/Obsidian.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 14)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Physical completion disagrees with metadata' }
Write-Host "PASS Obsidian L14: repeated full-route completion after the recovery switch approach, $($result.frames) frames"
