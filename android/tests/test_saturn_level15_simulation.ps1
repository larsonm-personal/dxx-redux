param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot 'android\temp\saturn_level15_simulation_test'
$missionFile = 'CD - Descent - Destination Saturn (USA).json'
& (Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1') `
    -MissionJson $missionFile -Level 15 -Repeat 2 -NoBuild:$NoBuild -OutputRoot $outputRoot
if ($LASTEXITCODE -ne 0) { throw 'Destination Saturn level 15 simulation failed' }
$records = @(Get-Content -LiteralPath (Join-Path $outputRoot "results\$($missionFile.Replace('.json', '.simulation.json'))") -Raw | ConvertFrom-Json)
$level = @($records.levels | Where-Object level_num -eq 15)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Destination Saturn level 15 did not complete deterministically'
}
Write-Host 'Destination Saturn level 15 simulation passed'
