# TEST-SUPPORT: owner=test_guidebot_route_regressions
param([switch]$NoBuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$outputRoot = Join-Path $repoRoot ('android/temp/guidebot_precision_recovery_test/run_' + (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
$missions = @(
    'TheOmicronProject.json', 'ironstar.json', 'eq-set.json',
    'Descent- Invertaus 1.2.json', 'd2xxl_downloads/ironstar.json', 'BelialSystemXL.json'
)
$levelFiles = @('dartdet2.rl2', 'alpha-h.rdl', 'eadgbe.rl2', 'fuego.rdl', '13final.rl2')

# These routes stalled when test-speed scaling preceded the precision cap
# Keep both Ironstar archives: their starting coordinates differ
& (Join-Path $repoRoot 'android/helpers/regenerate_all_guidebot_simulations.ps1') `
    -MissionJson $missions -LevelFileFilter $levelFiles -Repeat 2 -NoBuild:$NoBuild `
    -MaxParallel 2 -OutputRoot $outputRoot
if ($LASTEXITCODE -ne 0) { throw 'Precision recovery simulations reported infrastructure failures' }
$summary = Get-Content -LiteralPath (Join-Path $outputRoot 'summary.json') -Raw | ConvertFrom-Json
$expected = @(
    'BelialSystemXL.json|0|12|13final.rl2',
    'd2xxl_downloads/ironstar.json|0|7|alpha-h.rdl',
    'Descent- Invertaus 1.2.json|0|1|fuego.rdl',
    'eq-set.json|0|11|eadgbe.rl2',
    'ironstar.json|0|7|alpha-h.rdl',
    'TheOmicronProject.json|0|6|dartdet2.rl2'
)
if ($summary.selected_levels -ne 6 -or
    (Compare-Object $expected @($summary.selected_work_items))) {
    throw 'Precision recovery did not select the six expected archive/level variants'
}
$runs = @(Get-ChildItem -LiteralPath (Join-Path $outputRoot 'results') -File |
        Where-Object Name -Match '_run_[12]\.json$')
if ($runs.Count -ne 12) { throw 'Expected two native results for each precision recovery case' }
foreach ($run in $runs) {
    $result = Get-Content -LiteralPath $run.FullName -Raw | ConvertFrom-Json
    if ($result.status -ne 'confirmed' -or $result.radius.player -ne 310325 -or
        $result.radius.effective -ne $result.radius.player) {
        throw "Precision recovery must finish at full player radius: $($run.Name) ($($result.status))"
    }
}
foreach ($group in $runs | Group-Object { $_.Name -replace '_run_[12]\.json$', '' }) {
    if ($group.Count -ne 2 -or
        (Get-FileHash -LiteralPath $group.Group[0].FullName).Hash -ne
        (Get-FileHash -LiteralPath $group.Group[1].FullName).Hash) {
        throw "Precision recovery repeats differ: $($group.Name)"
    }
}
Write-Host 'GuideBot precision recovery passed: six levels, two identical runs each, full player radius'
