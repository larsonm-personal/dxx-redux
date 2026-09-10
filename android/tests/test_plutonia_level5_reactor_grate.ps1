#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_plutonia_level5_reactor_grate/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
$metadataOutput = Join-Path $output 'metadata_run'
& (Join-Path $repoRoot 'android/helpers/regenerate_all_mission_metadata_host.ps1') `
    -ArchiveNames plutonia.zip -MaxParallel 1 -NoBuild:$NoBuild -NoRegressionCopy -OutputRoot $metadataOutput
if ($LASTEXITCODE -ne 0) { throw 'Plutonia metadata generation failed' }
$metadataRoot = Join-Path $metadataOutput 'metadata'
$metadata = Get-Content -LiteralPath (Join-Path $metadataRoot 'plutonia.json') -Raw | ConvertFrom-Json
$level = @($metadata | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 5)
if ($level.Count -ne 1 -or $level[0].route_status -ne 'ok' -or
    @($level[0].route_steps | Where-Object required_weapon -eq 'guided_missile').Count) {
    throw 'Expected a complete ordinary route through the reactor door prerequisites'
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data/mission_files/plutonia.zip') -Destination $metadataRoot
$simulationOutput = Join-Path $output 'simulation'
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionMetadataRoot $metadataRoot -MissionJson plutonia.json -Level 5 -Repeat 2 `
    -MaxParallel 1 -NoBuild -OutputRoot $simulationOutput
if ($LASTEXITCODE -ne 0) { throw 'Plutonia reactor grate simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $simulationOutput 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeated physical runs' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Shoot switch trigger 8|blue key|Shoot switch trigger 16|red key|Fly-through trigger 2|Fly-through trigger 3|Reactor|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Plutonia L5 must complete both access triggers before destroying the reactor and exiting'
    }
    if ($result.radius.player -ne 310325 -or $result.radius.effective -ne $result.radius.player) {
        throw 'Reactor grate verification must retain the full player radius'
    }
    $log = Get-Content -LiteralPath (Join-Path $simulationOutput ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'verified primary shot actor_seg=275 target_seg=300 object=167' -or
        $log -match 'guided missile') {
        throw 'The reactor must be visible through the grate from the actual actor position'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Plutonia reactor grate repeats differ'
}
$normalized = Get-Content -LiteralPath (Join-Path $simulationOutput 'results/plutonia.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 5)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Metadata disagrees with physical completion' }
Write-Host "PASS Plutonia L5: ordinary reactor shot after both access triggers, repeated complete route, $($result.frames) frames"
