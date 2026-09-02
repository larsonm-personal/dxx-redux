param(
    [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$missionPath = Join-Path $repoRoot 'game_data\mission_files\Obsidian.json'
$outputRoot = Join-Path $repoRoot 'android\temp\obsidian_level1_route_test'

$mission = Get-Content -LiteralPath $missionPath -Raw | ConvertFrom-Json
$level = @($mission.levels | Where-Object { $_.level_num -eq 1 })[0]
$expectedMetadata = @(
    'Start',
    'blue key',
    'red key',
    'Shoot switch trigger 6',
    'Shoot switch trigger 5',
    'Fly-through trigger 8',
    'Reactor',
    'Exit'
)
if ($level.route_status -ne 'ok' -or
    $level.route_required_key_mask -ne 3 -or
    (@($level.route_steps.label) -join '|') -ne ($expectedMetadata -join '|')) {
    throw "Obsidian level 1 metadata route is incomplete: status=$($level.route_status) keys=$($level.route_required_key_mask)"
}

if (Test-Path -LiteralPath $outputRoot) {
    Remove-Item -LiteralPath $outputRoot -Recurse -Force
}
& $runner -Mode Headless -MissionJson Obsidian.json -Level 1 -Repeat 2 `
    -OutputRoot $outputRoot -NoBuild:$NoBuild

$resultFiles = @(Get-ChildItem -LiteralPath (Join-Path $outputRoot 'results') `
        -Filter '*_run_*.json' -File | Sort-Object Name)
if ($resultFiles.Count -ne 2) {
    throw "Expected two Obsidian level 1 route results, found $($resultFiles.Count)"
}
$expectedObjectives = @(
    'blue key',
    'red key',
    'Shoot switch trigger 6',
    'Shoot switch trigger 5',
    'Fly-through trigger 8',
    'Reactor',
    'Exit'
)
foreach ($resultFile in $resultFiles) {
    $result = Get-Content -LiteralPath $resultFile.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or
        (@($result.objectives.label) -join '|') -ne ($expectedObjectives -join '|')) {
        throw "Obsidian level 1 route was not confirmed: status=$($result.status) objectives=$(@($result.objectives.label) -join '|')"
    }
    $expectedRadius = [Math]::Max([int] $result.radius.player, [int] $result.radius.guidebot)
    if ([int] $result.radius.effective -ne $expectedRadius) {
        throw "Obsidian level 1 used the wrong navigation radius: effective=$($result.radius.effective) expected=$expectedRadius"
    }
}

Write-Host "Obsidian level 1 route confirmation passed: frames=$($result.frames)"
