# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot ("android\temp\guidebot_firststrike_live_objects\run_" + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
# Removing ordinary robots must not hide the red key in a higher object slot
& (Get-Process -Id $PID).Path -NoProfile -File (Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1') `
    -MissionJson 'FirstStrike.json' -Level 21 -Repeat 2 -OutputRoot $outputRoot -NoBuild:$NoBuild
if ($LASTEXITCODE -ne 0) { throw 'Live-object simulations reported infrastructure failures' }
$record = Get-Content -LiteralPath (Join-Path $outputRoot 'results\FirstStrike.simulation.json') -Raw | ConvertFrom-Json
$level = @($record.levels | Where-Object level_num -eq 21)
if ($level.Count -ne 1 -or $level[0].status -ne 'ok' -or $level[0].objectives[-1].n -ne 'exit') {
    throw 'Expected deterministic FirstStrike level 21 completion through the exit'
}
Write-Host 'FirstStrike live-object simulations passed'
