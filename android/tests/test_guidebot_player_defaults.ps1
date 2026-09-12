# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot ('android/temp/guidebot_player_defaults_test/run_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
& (Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1') `
    -Mode Headless -MissionJson 'dontpnic.json', 'Descent 1 to Descent 2 Conversion.json' `
    -Level -1, 9 -Repeat 2 -OutputRoot $outputRoot -NoBuild:$NoBuild
if ($LASTEXITCODE -ne 0) { throw 'Player-defaults simulations reported infrastructure failures' }
foreach ($case in @(@{ File = 'dontpnic'; Level = -1 }, @{ File = 'Descent 1 to Descent 2 Conversion'; Level = 9 })) {
    $record = Get-Content -LiteralPath (Join-Path $outputRoot "results\$($case.File).simulation.json") -Raw | ConvertFrom-Json
    $level = @($record.levels | Where-Object level_num -eq $case.Level)
    if ($level.Count -ne 1 -or $level[0].status -ne 'ok') {
        throw "Expected deterministic completion for $($case.File) level $($case.Level)"
    }
}
Write-Host 'GuideBot player-defaults simulations passed'
