param(
    [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$outputRoot = Join-Path $repoRoot 'android\temp\counterstrike_level6_blue_door_route_test'

if (Test-Path -LiteralPath $outputRoot) {
    Remove-Item -LiteralPath $outputRoot -Recurse -Force
}

& $runner -Mode Headless -MissionJson Counterstrike.json -Level 6 -Repeat 2 `
    -OutputRoot $outputRoot -NoBuild:$NoBuild

$resultFiles = @(Get-ChildItem -LiteralPath (Join-Path $outputRoot 'results') `
        -Filter '*_run_*.json' -File | Sort-Object Name)
if ($resultFiles.Count -ne 2) {
    throw "Expected two Counterstrike level 6 artifacts, found $($resultFiles.Count)"
}

$results = @($resultFiles | ForEach-Object {
        Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
    })
$expectedLabels = @(
    'blue key',
    'Fly-through trigger 0',
    'red key',
    'Fly-through trigger 4',
    'Reactor',
    'Exit'
)
foreach ($result in $results) {
    if ($result.status -ne 'confirmed') {
        throw "Counterstrike level 6 route was not confirmed: status=$($result.status), problem=$($result.problem)"
    }
    $actualLabels = @($result.objectives.label)
    if (Compare-Object $expectedLabels $actualLabels -SyncWindow 0) {
        throw "Counterstrike level 6 objectives changed: $($actualLabels -join ', ')"
    }
}
if ($results[0].frames -ne $results[1].frames -or
    $results[0].rng_end.simulation.state -ne $results[1].rng_end.simulation.state -or
    $results[0].rng_end.simulation.calls -ne $results[1].rng_end.simulation.calls) {
    throw 'Counterstrike level 6 repeated runs were not deterministic'
}

Write-Host "Counterstrike level 6 route passed twice: frames=$($results[0].frames)"
