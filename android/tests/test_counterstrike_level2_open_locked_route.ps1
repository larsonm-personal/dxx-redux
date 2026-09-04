param(
    [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$outputRoot = Join-Path $repoRoot 'android\temp\counterstrike_level2_open_locked_route_test'

if (Test-Path -LiteralPath $outputRoot) {
    Remove-Item -LiteralPath $outputRoot -Recurse -Force
}

& $runner -Mode Headless -MissionJson Counterstrike.json -Level 2 -Repeat 2 `
    -OutputRoot $outputRoot -NoBuild:$NoBuild

$resultFiles = @(Get-ChildItem -LiteralPath (Join-Path $outputRoot 'results') `
        -Filter '*_run_*.json' -File | Sort-Object Name)
if ($resultFiles.Count -ne 2) {
    throw "Expected two Counterstrike level 2 artifacts, found $($resultFiles.Count)"
}

$results = @($resultFiles | ForEach-Object {
        Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
    })
$expectedLabels = @(
    'Shoot switch trigger 17',
    'Shoot switch trigger 23',
    'Open hidden door',
    'Shoot switch trigger 21',
    'red key',
    'Shoot switch trigger 19',
    'Fly-through trigger 20',
    'Shoot switch trigger 3',
    'Reactor',
    'Exit'
)
foreach ($result in $results) {
    if ($result.status -ne 'confirmed') {
        throw "Counterstrike level 2 route was not confirmed: status=$($result.status), problem=$($result.problem)"
    }
    $actualLabels = @($result.objectives.label)
    if (Compare-Object $expectedLabels $actualLabels -SyncWindow 0) {
        throw "Counterstrike level 2 objectives changed: $($actualLabels -join ', ')"
    }
}
if ($results[0].frames -ne $results[1].frames -or
    $results[0].rng_end.simulation.state -ne $results[1].rng_end.simulation.state -or
    $results[0].rng_end.simulation.calls -ne $results[1].rng_end.simulation.calls) {
    throw 'Counterstrike level 2 repeated runs were not deterministic'
}

Write-Host "Counterstrike level 2 route passed twice: frames=$($results[0].frames)"
