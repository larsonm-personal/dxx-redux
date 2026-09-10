#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_lostlvls_directional_unlock/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
$metadataOutput = Join-Path $output 'metadata_run'
& (Join-Path $repoRoot 'android/helpers/regenerate_all_mission_metadata_host.ps1') `
    -ArchiveNames Lostlvls.zip -MaxParallel 1 -NoBuild:$NoBuild -NoRegressionCopy -OutputRoot $metadataOutput
if ($LASTEXITCODE -ne 0) { throw 'Lost Levels metadata generation failed' }
$metadataRoot = Join-Path $metadataOutput 'metadata'
$metadata = Get-Content -LiteralPath (Join-Path $metadataRoot 'Lostlvls.json') -Raw | ConvertFrom-Json
$level = @($metadata | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 17)
if ($level.Count -ne 1 -or $level[0].route_status -ne 'ok' -or
    @($level[0].route_steps | Where-Object required_weapon -eq 'guided_missile').Count -or
    @($level[0].route_steps | Where-Object { $_.wall -eq 34 -and $_.activation_kind -eq 'open_hidden_door' }).Count -ne 1) {
    throw 'Expected a complete ordinary route that explicitly opens the unlocked door face'
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data/mission_files/Lostlvls.zip') -Destination $metadataRoot
$simulationOutput = Join-Path $output 'simulation'
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionMetadataRoot $metadataRoot -MissionJson Lostlvls.json -Level 17 -Repeat 2 `
    -MaxParallel 1 -NoBuild -OutputRoot $simulationOutput
if ($LASTEXITCODE -ne 0) { throw 'Lost Levels directional unlock simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $simulationOutput 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeated physical runs' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    $sequence = 'Shoot switch trigger 3|Open door|Fly-through trigger 7|Shoot switch trigger 9|blue key|gold key|Open door|Shoot switch trigger 29|red key|Reactor|Exit'
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne $sequence) {
        throw 'Lost Levels L17 must complete the door prerequisites, keys, reactor and exit'
    }
    if ($result.radius.player -ne 426296 -or $result.radius.effective -ne $result.radius.player) {
        throw 'Directional unlock verification must retain the full player radius'
    }
    $log = Get-Content -LiteralPath (Join-Path $simulationOutput ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'flare wall=34 seg=89 side=5 actor_seg=83' -or
        $log -match 'close flare fallback wall=34|guided missile') {
        throw 'The unlocked face must open through ordinary projectile flight'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Lost Levels directional unlock repeats differ'
}
$normalized = Get-Content -LiteralPath (Join-Path $simulationOutput 'results/Lostlvls.simulation.json') -Raw | ConvertFrom-Json
$entry = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 17)
if ($entry.Count -ne 1 -or $entry[0].status -ne 'ok') { throw 'Metadata disagrees with physical completion' }
Write-Host "PASS Lost Levels L17: directional unlock, ordinary door shot and repeated complete route, $($result.frames) frames"
