param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot ("android\temp\guidebot_successive_frontiers\run_" + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
# Mercury Core crosses several keyed frontiers while pursuing the same red key
& (Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1') `
    -MissionJson 'descent.fan_d2_conversion.json' -Level 7 -Repeat 2 -OutputRoot $outputRoot -NoBuild:$NoBuild
if ($LASTEXITCODE -ne 0) { throw 'Successive-frontier simulations reported infrastructure failures' }
$record = Get-Content -LiteralPath (Join-Path $outputRoot 'results\descent.fan_d2_conversion.simulation.json') -Raw | ConvertFrom-Json
$level = @($record.levels | Where-Object level_num -eq 7)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
    throw 'Expected deterministic completion of Descent conversion level 7'
}
Write-Host 'GuideBot successive-frontier simulations passed'
