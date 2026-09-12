#!/usr/bin/env pwsh
# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot "android/temp/test_eaf2_reactor_access/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"
$metadataOutput = Join-Path $output 'metadata_run'
& (Join-Path $repoRoot 'android/helpers/regenerate_all_mission_metadata_host.ps1') `
    -ArchiveNames EAF2.zip -MaxParallel 1 -NoBuild:$NoBuild -NoRegressionCopy -OutputRoot $metadataOutput
if ($LASTEXITCODE -ne 0) { throw 'EAF2 metadata generation failed' }
$metadataRoot = Join-Path $metadataOutput 'metadata'
$metadata = Get-Content -LiteralPath (Join-Path $metadataRoot 'EAF2.json') -Raw | ConvertFrom-Json
$level = @($metadata | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 2)
if ($level.Count -ne 1 -or $level[0].route_status -ne 'ok' -or
    @($level[0].route_steps | Where-Object required_weapon -eq 'guided_missile').Count) {
    throw 'Expected a complete ordinary route to the reactor firing area'
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data/mission_files/EAF2.zip') -Destination $metadataRoot
$simOutput = Join-Path $output 'simulation'
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionMetadataRoot $metadataRoot -MissionJson EAF2.json -Level 2 -Repeat 2 `
    -MaxParallel 1 -NoBuild -OutputRoot $simOutput
if ($LASTEXITCODE -ne 0) { throw 'EAF2 simulation failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $simOutput 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeated physical runs' }
$first = $null
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Shoot switch trigger 3|Destroy robot carrying red key|Shoot switch trigger 4|Destroy robot carrying gold key|Shoot switch trigger 5|Destroy robot carrying blue key|Shoot switch trigger 0|Shoot switch trigger 1|Shoot switch trigger 2|Reactor|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'EAF2 L2 did not complete its access prerequisites, reactor and exit'
    }
    $log = Get-Content -LiteralPath (Join-Path $simOutput ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'verified primary shot actor_seg=553 target_seg=569 object=66') {
        throw 'Expected the actual firing pose outside the permanent reactor grate'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Nondeterministic EAF2 L2 completion' }
}
$normalized = Get-Content -LiteralPath (Join-Path $simOutput 'results/EAF2.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 2)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Metadata disagrees with physical completion' }
Write-Host "PASS EAF2 L2: repeated ordinary access, reactor shot and exit, $($result.frames) frames"
