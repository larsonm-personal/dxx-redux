#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$output = Join-Path $repoRoot ('android/temp/test_lostlvls_key_contact/' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionJson Lostlvls.json -Level 23 -Repeat 2 -MaxParallel 1 -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Lost Levels pickup simulation infrastructure failed' }
$files = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($files.Count -ne 2) { throw 'Expected two repeated physical runs' }
foreach ($file in $files) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne
        'Reactor|blue key|gold key|red key|Exit') {
        throw 'Lost Levels L23 must collect every key after the reactor and reach the exit'
    }
    if ($result.radius.player -ne 426296 -or $result.radius.effective -ne $result.radius.player) {
        throw 'Pickup verification must retain this fixture''s full player radius'
    }
}
if ((Get-FileHash $files[0].FullName).Hash -ne (Get-FileHash $files[1].FullName).Hash) {
    throw 'Lost Levels key-contact repeats differ'
}
$normalized = Get-Content -LiteralPath (Join-Path $output 'results/Lostlvls.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized | Where-Object target_index -eq 0 | ForEach-Object levels | Where-Object level_num -eq 23)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') { throw 'Lost Levels key contact disagrees with the metadata route' }
Write-Host "PASS Lost Levels L23: full-radius key pickups and repeated complete route, $($result.frames) frames"

# A broad final-key steering change altered this healthy approach and broke the later route
$preservationOutput = Join-Path $output 'af6'
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionJson af_d1_beta.json -Level 6 -Repeat 2 -MaxParallel 1 -NoBuild -OutputRoot $preservationOutput
if ($LASTEXITCODE -ne 0) { throw 'AF D1 beta pickup preservation infrastructure failed' }
$preservationFiles = @(Get-ChildItem -LiteralPath (Join-Path $preservationOutput 'results') -Filter '*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($preservationFiles.Count -ne 2) { throw 'Expected two AF D1 beta preservation runs' }
foreach ($file in $preservationFiles) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or ($result.objectives.label -join '|') -cne 'blue key|gold key|red key|Reactor|Exit') {
        throw 'A valid pickup approach must preserve AF D1 beta L6 completion'
    }
}
if ((Get-FileHash $preservationFiles[0].FullName).Hash -ne (Get-FileHash $preservationFiles[1].FullName).Hash) {
    throw 'AF D1 beta pickup preservation repeats differ'
}
Write-Host "PASS AF D1 beta L6: repeated complete route preserved, $($result.frames) frames"
