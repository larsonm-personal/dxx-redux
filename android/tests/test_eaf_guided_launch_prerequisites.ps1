#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_eaf_guided_launch_prerequisites/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
$metadataOutput = Join-Path $output 'metadata_run'
& (Join-Path $repoRoot 'android/helpers/regenerate_all_mission_metadata_host.ps1') `
    -ArchiveNames EAF.zip -MaxParallel 1 -NoBuild:$NoBuild -NoRegressionCopy -OutputRoot $metadataOutput
if ($LASTEXITCODE -ne 0) { throw 'EAF metadata generation failed' }
$metadataRoot = Join-Path $metadataOutput 'metadata'
$metadata = Get-Content -LiteralPath (Join-Path $metadataRoot 'EAF.json') -Raw | ConvertFrom-Json
$level = @($metadata | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 2)
if ($level.Count -ne 1 -or $level[0].route_status -ne 'ok') { throw 'Expected a complete predicted EAF L2 route' }
$switches = @($level[0].route_steps | Where-Object kind -eq 'trigger')
if (($switches.trigger -join ',') -cne '0,1,2,3,4,5') { throw 'Launch access prerequisites must precede the reactor switches' }
foreach ($step in $switches) {
    if ($step.required_weapon -cne 'guided_missile' -or $step.label -cne 'Shoot using guided missile') {
        throw 'The predicted curved shots must retain their explicit equipment requirement'
    }
}
if (($level[0].route_steps.kind -join ',') -cne 'start,trigger,trigger,trigger,trigger,trigger,trigger,reactor,exit') {
    throw 'The predicted route must include the reactor and exit after the switches'
}
Copy-Item -LiteralPath (Join-Path $repoRoot 'game_data/mission_files/EAF.zip') -Destination $metadataRoot
$simulationOutput = Join-Path $output 'simulation'
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionMetadataRoot $metadataRoot -MissionJson EAF.json -Level 2 -Repeat 2 `
    -MaxParallel 1 -NoBuild -OutputRoot $simulationOutput
if ($LASTEXITCODE -ne 0) { throw 'EAF guided launch simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $simulationOutput 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeated simulation results' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'unsupported' -or
        $result.problem -cne 'guided missile objective requires equipment and flight verification') {
        throw 'Geometry prediction must not claim verified guided missile flight'
    }
    $log = Get-Content -LiteralPath (Join-Path $simulationOutput ('logs/' + $file.BaseName + '.log')) -Raw
    if ($log -notmatch 'guided wall=54 trigger=0 launch=455 points=5 instruction=shoot using guided missile') {
        throw 'The live goal must expose the first predicted guided shot and its instruction'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Repeated EAF guided launch results differ'
}
# This verifies dependency planning and annotations, not equipment acquisition or missile flight
Write-Host 'PASS EAF L2: complete predicted prerequisites and explicit unverified guided-flight status'
