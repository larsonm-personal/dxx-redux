param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot 'android\temp\guidebot_long_path_test'
& (Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1') `
    -Mode Headless -MissionJson 'd2xxl_downloads/revodrav.json', 'bratmaze.json' `
    -Level 1, 18, 23 -Repeat 2 -OutputRoot $outputRoot -NoBuild:$NoBuild
if ($LASTEXITCODE -ne 0) { throw 'Long-path simulation run reported infrastructure failures' }
$summary = Get-Content -LiteralPath (Join-Path $outputRoot 'summary.json') -Raw | ConvertFrom-Json
if ($summary.selected_levels -ne 4 -or $summary.infrastructure_failures.Count -ne 0) {
    throw 'Expected four long-path levels without infrastructure failures'
}
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $outputRoot 'results') -Filter '*.simulation.json') {
    $records = @(Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json)
    if (@($records.levels | Where-Object status -eq 'nondeterministic').Count) {
        throw "Repeated long-path simulations diverged: $($file.Name)"
    }
}
Write-Host 'GuideBot long-path simulation tests passed'
