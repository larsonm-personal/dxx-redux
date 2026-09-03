param(
    [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$outputRoot = Join-Path $repoRoot 'android\temp\counterstrike_level10_key_carriers_test'

if (Test-Path -LiteralPath $outputRoot) {
    Remove-Item -LiteralPath $outputRoot -Recurse -Force
}

& $runner -Mode Headless -MissionJson Counterstrike.json -Level 10 -Repeat 2 `
    -OutputRoot $outputRoot -NoBuild:$NoBuild

$resultFile = Get-ChildItem -LiteralPath (Join-Path $outputRoot 'results') `
    -Filter '*_run_1.json' -File | Select-Object -First 1
if (-not $resultFile) {
    throw 'Counterstrike level 10 route artifact is missing'
}

$result = Get-Content -LiteralPath $resultFile.FullName -Raw | ConvertFrom-Json
$labels = @($result.objectives.label)
foreach ($expected in @(
        'Destroy robot carrying blue key',
        'Destroy robot carrying gold key',
        'Destroy robot carrying red key',
        'Open hidden door',
        'Reactor',
        'Exit'
    )) {
    if ($expected -notin $labels) {
        throw "Counterstrike level 10 did not complete $expected`: status=$($result.status), labels=$($labels -join ', ')"
    }
}
if ($result.status -ne 'confirmed') {
    throw "Counterstrike level 10 route was not confirmed: status=$($result.status), problem=$($result.problem)"
}
$simulationFile = Join-Path $outputRoot 'results\Counterstrike.simulation.json'
$simulation = Get-Content -LiteralPath $simulationFile -Raw | ConvertFrom-Json
$level = @($simulation.levels | Where-Object { $_.level_num -eq 10 })[0]
if ($level.status -ne 'ok') {
    throw "Counterstrike level 10 normalized regression result is $($level.status): $($level.problem)"
}

Write-Host "Counterstrike level 10 key-carrier route passed: frames=$($result.frames)"
