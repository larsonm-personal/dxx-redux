param(
    [switch] $NoBuild,
    [string] $HeadlessExecutable,
    [string] $HogDir
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android/helpers/test_host_platform.ps1')
$helper = Join-Path $repoRoot 'android\helpers\guidebot_simulation_regression.ps1'
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$outputRoot = Join-Path $repoRoot ('android/temp/guidebot-timeout-' + [guid]::NewGuid().ToString('N'))
$exe = if ($HeadlessExecutable) {
    $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($HeadlessExecutable)
} else {
    Join-RegressionPath $repoRoot 'buildd2' 'main' (Get-RegressionHostExecutableNames -BaseName 'dxx-redux-d2-headless-route')[0]
}
if (-not $HogDir) { $HogDir = Join-Path $repoRoot 'game_data/CD images/Descent II (USA) (v1.1)/data_tracks/d2data' }
. $helper

function Assert-Equal {
    param([object]$Expected, [object]$Actual, [string]$Message)
    if ($Expected -ne $Actual) { throw "$Message expected=$Expected actual=$Actual" }
}

Assert-Equal 30 (Get-GuidebotSimulationTimeLimitSeconds ([pscustomobject]@{ travel_distance = 0 })) 'Minimum budget changed'
Assert-Equal 31 (Get-GuidebotSimulationTimeLimitSeconds ([pscustomobject]@{ travel_distance = 1 })) 'Distance credit must round up'
Assert-Equal 183 (Get-GuidebotSimulationTimeLimitSeconds ([pscustomobject]@{ travel_distance = 4579.9 })) 'Obsidian level 1 budget changed'
Assert-Equal 300 (Get-GuidebotSimulationTimeLimitSeconds ([pscustomobject]@{ travel_distance = 21515.3 })) 'Maximum budget changed'
Assert-Equal 30 (Get-GuidebotSimulationTimeLimitSeconds ([pscustomobject]@{ travel_distance = [double]::NaN })) 'Invalid distance fallback changed'

& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $outputRoot -DirectoryPrefix 'guidebot-timeout-' -MinimumFreeSpaceGB 0.01
New-Item -ItemType Directory -Path $outputRoot | Out-Null
$fixtureLock = $null
try {
    $fixtureLock = [IO.File]::Open((Join-Path $outputRoot 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    $dryRunPath = Join-Path $outputRoot 'dry-run.json'
    & $runner -MissionJson Counterstrike.json -Level 1 -HogDir $outputRoot -DryRun -DryRunJsonOut $dryRunPath | Out-Null
    $dryRun = @(Get-Content -LiteralPath $dryRunPath -Raw | ConvertFrom-Json)
    Assert-Equal 1 $dryRun.Count 'Focused dry run count changed'
    Assert-Equal 107 $dryRun[0].simulation_time_limit_seconds 'Runner did not use the distance-scaled budget'

    # Exercise the generated Android script with a slow-emulator wall-clock budget
    $runnerAst = [Management.Automation.Language.Parser]::ParseFile($runner, [ref]$null, [ref]$null)
    $headedFunction = $runnerAst.Find({
            param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'New-GuidebotHeadedScript'
        }, $true)
    . ([scriptblock]::Create($headedFunction.Extent.Text))
    $workItem = [pscustomobject]@{
        Mission = [pscustomobject]@{ mission_filename = 'd2' }
        Level = [pscustomobject]@{ level_num = 1 }
        D1InD2 = $false
        EngineLevelNumber = 1
        SimulationTimeLimitSeconds = 107
    }
    $headedScriptPath = Join-Path $outputRoot 'headed.jsonc'
    foreach ($wallBudget in @(600, 10)) {
        $LevelTimeoutSeconds = $wallBudget
        New-GuidebotHeadedScript -WorkItem $workItem -Path $headedScriptPath
        $steps = Get-Content -LiteralPath $headedScriptPath -Raw | ConvertFrom-Json
        Assert-Equal '107' ($steps | Where-Object field -eq 'start_route_confirmation').value 'Wall-clock budget changed the simulation budget'
        $expectedWait = if ($wallBudget -eq 600) { 600000 } else { 122000 }
        Assert-Equal $expectedWait ($steps | Where-Object field -eq 'route_confirmation_terminal').timeout_ms 'Headed wait ignores the wall-clock or simulation budget'
    }

    Write-Host 'GuideBot timeout calculation and generated-script checks passed'
    # Native data lookup accepts the uppercase filenames used by retail CDs
    $availableNames = @(Get-ChildItem -LiteralPath $HogDir -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
    $missing = @('descent2.hog', 'descent2.ham', 'groupa.pig' | Where-Object { $_ -notin $availableNames })
    if ($missing.Count) {
        Write-Host "RESULT: SKIP (native timeout requires missing D2 assets in ${HogDir}: $($missing -join ', '))"
        exit 2
    }
    if (-not $NoBuild) {
        Invoke-RegressionHostBuild -RepoRoot $repoRoot -Target d2
    }
    $engineResultPath = Join-Path $outputRoot 'one-second-engine-result.json'
    & $exe -hogdir $hogDir -mission d2 -level 1 `
        -route-confirm-timeout-seconds 1 -route-confirm-json-out $engineResultPath | Out-Null
    if ($LASTEXITCODE -ne 2) { throw "Expected controlled route timeout exit 2, received $LASTEXITCODE" }
    $engineResult = Get-Content -LiteralPath $engineResultPath -Raw | ConvertFrom-Json
    Assert-Equal 'timeout' $engineResult.status 'Engine did not apply the requested timeout'
    Assert-Equal 61 $engineResult.frames 'Engine timeout frame changed'
    if ($engineResult.problem -ne 'route confirmation exceeded 1-second simulation budget') {
        throw "Unexpected engine timeout problem: $($engineResult.problem)"
    }

    Write-Host 'GuideBot distance-scaled simulation timeout policy passed'
} finally {
    if ($fixtureLock) { $fixtureLock.Dispose() }
    if (Test-Path -LiteralPath $outputRoot) { Remove-Item -LiteralPath $outputRoot -Recurse -Force }
}
