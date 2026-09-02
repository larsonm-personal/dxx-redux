param(
    [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$outputRoot = Join-Path $repoRoot 'android\temp\counterstrike_level1_route_test'

if (Test-Path -LiteralPath $outputRoot) {
    Remove-Item -LiteralPath $outputRoot -Recurse -Force
}

& $runner -Mode Headless -MissionJson Counterstrike.json -Level 1 -Repeat 2 `
    -OutputRoot $outputRoot -NoBuild:$NoBuild

$resultFile = Get-ChildItem -LiteralPath (Join-Path $outputRoot 'results') `
    -Filter '*_run_1.json' -File | Select-Object -First 1
$logFile = Get-ChildItem -LiteralPath (Join-Path $outputRoot 'logs') `
    -Filter '*_run_1.log' -File | Select-Object -First 1
if (-not $resultFile -or -not $logFile) {
    throw 'Counterstrike level 1 route artifacts are missing'
}

$result = Get-Content -LiteralPath $resultFile.FullName -Raw | ConvertFrom-Json
$log = Get-Content -LiteralPath $logFile.FullName -Raw
if ($result.status -ne 'confirmed' -or @($result.objectives).Count -ne 3) {
    throw "Counterstrike level 1 route was not confirmed: status=$($result.status) objectives=$(@($result.objectives).Count)"
}
$reactor = @($result.objectives | Where-Object { $_.label -eq 'Reactor' })[0]
$exit = @($result.objectives | Where-Object { $_.label -eq 'Exit' })[0]
if (-not $reactor -or -not $exit -or
    ([int] $exit.frame - [int] $reactor.frame) -ge 180) {
    throw "Counterstrike level 1 paused before its exit: reactor=$($reactor.frame) exit=$($exit.frame)"
}
if ($log -notmatch 'ROUTE-CONFIRM flare wall=\d+ seg=120' -or
    $log -notmatch 'ROUTE-CONFIRM extend_frontier attempt=1 actor_seg=120 semantic=159 navigation=159') {
    throw 'Counterstrike level 1 did not flare and immediately route through its red-door frontier'
}

Write-Host "Counterstrike level 1 route confirmation passed: frames=$($result.frames)"
