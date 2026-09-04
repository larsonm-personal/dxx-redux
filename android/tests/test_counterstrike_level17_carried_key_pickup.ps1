param(
    [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$outputRoot = Join-Path $repoRoot 'android\temp\counterstrike_level17_carried_key_pickup_test'

if (Test-Path -LiteralPath $outputRoot) {
    Remove-Item -LiteralPath $outputRoot -Recurse -Force
}

& $runner -Mode Headless -MissionJson Counterstrike.json -Level 17 -Repeat 2 `
    -OutputRoot $outputRoot -NoBuild:$NoBuild

$resultFiles = @(Get-ChildItem -LiteralPath (Join-Path $outputRoot 'results') `
        -Filter '*_run_*.json' -File | Sort-Object Name)
if ($resultFiles.Count -ne 2) {
    throw "Expected two Counterstrike level 17 artifacts, found $($resultFiles.Count)"
}

$results = @($resultFiles | ForEach-Object {
        Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json
    })
$expectedObjectives = @(
    'Destroy robot carrying blue key',
    'gold key',
    'Fly-through trigger 14',
    'Shoot switch trigger 16',
    'red key',
    'Pass through trigger 22',
    'Reactor',
    'Exit'
)
foreach ($result in $results) {
    $actualLabels = @($result.objectives.label)
    if ($result.status -ne 'confirmed' -or
        (Compare-Object $expectedObjectives $actualLabels -SyncWindow 0)) {
        throw "Counterstrike level 17 did not complete its route: status=$($result.status), objectives=$($actualLabels -join ', ')"
    }
}
if ($results[0].frames -ne $results[1].frames -or
    $results[0].rng_end.simulation.state -ne $results[1].rng_end.simulation.state -or
    $results[0].rng_end.simulation.calls -ne $results[1].rng_end.simulation.calls) {
    throw 'Counterstrike level 17 repeated runs were not deterministic'
}

Write-Output "Counterstrike level 17 route passed twice: frames=$($results[0].frames)"
