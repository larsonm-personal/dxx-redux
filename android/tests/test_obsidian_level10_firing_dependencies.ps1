#!/usr/bin/env pwsh
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$runner = Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1'
$output = Join-Path $repoRoot "android/temp/test_obsidian_level10_firing_dependencies/run_$(Get-Date -Format 'yyyyMMdd_HHmmss_fff')"

$metadata = Get-Content (Join-Path $repoRoot 'game_data/mission_files/Obsidian.json') -Raw | ConvertFrom-Json
$plannedLevel = @($metadata.levels | Where-Object level_num -eq 10)
if ($plannedLevel.Count -ne 1 -or $plannedLevel[0].route_status -ne 'ok') {
    throw 'Obsidian level 10 metadata must include a complete route through the firing prerequisites'
}
& $runner -Mode Headless `
    -MissionJson Obsidian.json -Level 10,14 -Repeat 2 -MaxParallel 1 `
    -NoBuild:$NoBuild -OutputRoot $output
if ($LASTEXITCODE -ne 0) { throw 'Obsidian level 10 runner failed' }

$results = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Obsidian.json_0_10_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($results.Count -ne 2) { throw "Expected two results, found $($results.Count)" }
$expected = @(
    'Destroy robot carrying blue key',
    'Destroy robot carrying gold key',
    'Shoot switch trigger 8',
    'Shoot switch trigger 14',
    'Open door',
    'Shoot switch trigger 0',
    'Shoot switch trigger 7',
    'Shoot switch trigger 9',
    'Shoot switch trigger 6',
    'red key',
    'Reactor',
    'Exit'
)
$first = $null
foreach ($file in $results) {
    $result = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed') {
        throw "Obsidian level 10 did not reach the exit: $($result.status), $($result.problem)"
    }
    if (($result.objectives.label -join '|') -cne ($expected -join '|')) {
        throw 'Obsidian level 10 did not activate the door and switch prerequisites and complete its route in order'
    }
    $signature = $result | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $first) { $first = $signature }
    elseif ($signature -cne $first) { throw 'Obsidian level 10 simulation is not deterministic' }
}
# Level 14 must recover when momentum carries the actor outside its new path
$recoveryResults = @(Get-ChildItem -LiteralPath (Join-Path $output 'results') `
        -Filter 'Obsidian.json_0_14_*_run_*.json' |
        Where-Object Name -Match '_run_[0-9]+\.json$' | Sort-Object Name)
if ($recoveryResults.Count -ne 2) { throw 'Expected two Obsidian level 14 recovery results' }
$recoverySignature = $null
foreach ($file in $recoveryResults) {
    $recovery = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    if ($recovery.status -ne 'confirmed') { throw "Obsidian level 14 did not reach the exit: $($recovery.status)" }
    $signature = $recovery | ConvertTo-Json -Depth 30 -Compress
    if ($null -eq $recoverySignature) { $recoverySignature = $signature }
    elseif ($signature -cne $recoverySignature) { throw 'Obsidian level 14 recovery is not deterministic' }
}
$normalized = Get-Content (Join-Path $output 'results/Obsidian.simulation.json') -Raw | ConvertFrom-Json
$level = @($normalized.levels | Where-Object { $_.level_num -in @(10,14) })
if ($level.Count -ne 2 -or @($level | Where-Object status -ne 'ok').Count) {
    throw 'Obsidian level 10 physical objectives disagree with its metadata route'
}
Write-Host "PASS Obsidian levels 10 and 14: $($result.frames)/$($recovery.frames) frames, deterministic, metadata agrees"
