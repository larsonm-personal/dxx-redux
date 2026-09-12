# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot ("android\temp\guidebot_waypoint_clearance\run_" + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
# The conversion's yellow-key contact point used to intersect the alcove ceiling
& (Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1') `
    -MissionJson 'descent.fan_d2_conversion.json', 'FirstStrike.json' -Level 8, 12 -Repeat 2 -OutputRoot $outputRoot -NoBuild:$NoBuild
if ($LASTEXITCODE -ne 0) { throw 'Waypoint-clearance simulations reported infrastructure failures' }
foreach ($mission in @('descent.fan_d2_conversion', 'FirstStrike')) {
    $record = Get-Content -LiteralPath (Join-Path $outputRoot "results\$mission.simulation.json") -Raw | ConvertFrom-Json
    foreach ($number in @(8, 12)) {
        $level = @($record.levels | Where-Object level_num -eq $number)
        if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
            throw "Expected deterministic completion of $mission level $number"
        }
    }
}
Write-Host 'GuideBot waypoint-clearance simulations passed'
