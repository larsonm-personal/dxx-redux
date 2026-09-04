param(
    [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$outputRoot = Join-Path $repoRoot 'android\temp\vertigo_level6_blue_door_route_test'
$mission = 'CD - Descent II - The Vertigo Series (USA).json'

if (Test-Path -LiteralPath $outputRoot) {
    Remove-Item -LiteralPath $outputRoot -Recurse -Force
}

& $runner -Mode Headless -MissionJson $mission -Level 6 -Repeat 2 `
    -OutputRoot $outputRoot -NoBuild:$NoBuild

$resultFiles = @(Get-ChildItem -LiteralPath (Join-Path $outputRoot 'results') `
        -Filter '*_run_*.json' -File | Sort-Object Name)
if ($resultFiles.Count -ne 2) {
    throw "Expected two Vertigo level 6 artifacts, found $($resultFiles.Count)"
}

$results = @($resultFiles | ForEach-Object {
        Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
    })
$expectedPrefix = @('blue key', 'gold key', 'red key')
foreach ($result in $results) {
    $actualLabels = @($result.objectives.label)
    if ($actualLabels.Count -lt $expectedPrefix.Count -or
        (Compare-Object $expectedPrefix $actualLabels[0..2] -SyncWindow 0)) {
        throw "Vertigo level 6 did not cross the blue door: status=$($result.status), objectives=$($actualLabels -join ', ')"
    }
}
if ($results[0].frames -ne $results[1].frames -or
    $results[0].rng_end.simulation.state -ne $results[1].rng_end.simulation.state -or
    $results[0].rng_end.simulation.calls -ne $results[1].rng_end.simulation.calls) {
    throw 'Vertigo level 6 repeated runs were not deterministic'
}

Write-Host "Vertigo level 6 crossed the blue door twice: frames=$($results[0].frames)"
