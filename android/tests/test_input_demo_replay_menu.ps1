#!/usr/bin/env pwsh

[Diagnostics.CodeAnalysis.SuppressMessageAttribute('PSAvoidOverwritingBuiltInCmdlets', '', Justification = 'Mock menu input for unattended replay tests')]
param()

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$androidRoot = Split-Path $PSScriptRoot -Parent
$sourceRepo = Split-Path $androidRoot -Parent
$runner = Join-Path $PSScriptRoot 'run_input_demo_replay.ps1'
. (Join-Path $androidRoot 'helpers/input_demo_replay_menu.ps1')
. (Join-Path $androidRoot 'helpers/run_all_tests_profile_menu.ps1')
. (Join-Path $androidRoot 'helpers/input_demo_fixture_catalog.ps1')
. (Join-Path $androidRoot 'helpers/powershell_compat.ps1')
. (Join-Path $androidRoot 'helpers/test_host_platform.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Pattern)
    try { & $Action | Out-Null } catch {
        if ($_.Exception.Message -notlike $Pattern) { throw }
        return
    }
    throw "Expected failure: $Pattern"
}

function Set-MenuTestAnswers {
    param([string[]]$Answers)
    $script:menuTestAnswers = [Collections.Generic.Queue[string]]::new()
    foreach ($answer in $Answers) { $script:menuTestAnswers.Enqueue($answer) }
}

function Invoke-MenuTestWrapper {
    param([hashtable]$Options, [string[]]$Answers = @())
    Set-MenuTestAnswers -Answers $Answers
    $text = & $runner @Options 6>&1 | Out-String
    Assert-True ($LASTEXITCODE -eq 0) 'Replay wrapper should exit successfully'
    Assert-True ($script:menuTestAnswers.Count -eq 0) 'Replay wrapper left menu answers unused'
    return $text
}

function Write-MenuTestDemo {
    param([string]$Path, [string]$Game = 'd1')
    @{ type = 'header'; game = $Game; mission = 'test'; level = 1; start_mode = 'new_level'; version = 5; frame_count = 1 } |
        ConvertTo-Json -Compress | Set-Content -LiteralPath $Path -Encoding utf8
}

# Load actual wrapper functions without executing its game/build entry point
$tokens = $null
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($runner, [ref]$tokens, [ref]$parseErrors)
Assert-True ($parseErrors.Count -eq 0) 'Replay wrapper must parse'
foreach ($name in @('ConvertFrom-JsonLine', 'Test-DemoJsonRecordLine', 'Get-DemoApproximateReplayFps',
        'Get-SearchRoots', 'Get-DemoHeader', 'Get-RelativeRepoPath', 'Get-DemoCandidates', 'Get-LaunchMode',
        'Get-LaunchArguments', 'Get-HeadlessConsoleExe', 'Test-CanUseHeadlessConsoleRunner', 'Resolve-ReplayRunnerSelection')) {
    $definition = $ast.Find({ param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
        }, $true)
    Assert-True ($null -ne $definition) "Missing replay function: $name"
    . ([scriptblock]::Create($definition.Extent.Text))
}

Assert-True ((Select-RunAllTestsProfile -ReadChoice { 'r' }) -eq 'ReplayDemo') 'Suite menu should select demo replay'
foreach ($case in @(@('1', 'd1'), @('2', 'd2'), @('3', 'd1-in-d2'), @('', 'd1'), @('Q', 'cancel'))) {
    Assert-True ((Select-InputDemoReplayEngine -ReadChoice { $case[0] }) -eq $case[1]) 'Incorrect engine selection'
}
foreach ($case in @(@('1', 'headed'), @('2', 'headless'), @('', 'headed'), @('Q', 'cancel'))) {
    Assert-True ((Select-InputDemoReplayDisplay -ReadChoice { $case[0] }) -eq $case[1]) 'Incorrect playback selection'
}
$choices = [System.Collections.Generic.Queue[string]]::new()
$choices.Enqueue('invalid')
$choices.Enqueue('2')
Assert-True ((Select-InputDemoReplayDisplay -ReadChoice { $choices.Dequeue() }) -eq 'headless') 'Invalid choices should retry'

# Exercise the real wrapper's header-based filtering and the suite's early
# cancellation path, without starting a game, build or test infrastructure
function Read-Host {
    param([string]$Prompt)
    if ($menuTestAnswers.Count -eq 0) { throw "Unexpected prompt: $Prompt" }
    return $menuTestAnswers.Dequeue()
}

$fixtureRoot = Join-Path $androidRoot ('temp/input_demo_menu_test/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $androidRoot 'helpers/retain-recent-artifacts.ps1') -Artifacts $fixtureRoot -DirectoryPrefix run_
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
try {
    $searchRoot = Join-Path $fixtureRoot 'recordings with spaces'
    New-Item -ItemType Directory -Path $searchRoot | Out-Null
    foreach ($game in @('d1', 'd2')) {
        # Names deliberately do not encode the recorded game
        $fileName = if ($game -eq 'd1') { 'first.dximdemo' } else { 'second.dximdemo' }
        Write-MenuTestDemo -Path (Join-Path $searchRoot $fileName) -Game $game
    }
    foreach ($engine in @('1', '2', '3')) {
        $output = Invoke-MenuTestWrapper -Answers @($engine) -Options @{ Interactive = $true; ListOnly = $true; SearchRoot = $searchRoot }
        $wanted = if ($engine -eq '2') { 'second.dximdemo' } else { 'first.dximdemo' }
        $unwanted = if ($engine -eq '2') { 'first.dximdemo' } else { 'second.dximdemo' }
        Assert-True ($output.Contains($wanted) -and -not $output.Contains($unwanted)) "Wrong recording filter for engine $engine"
        Assert-True (-not $output.Contains('d1-public-flight')) 'An explicit search root must replace default roots'
    }

    # Inspect the committed corpus, then exercise real default discovery in an
    # isolated repository with old recordings and deliberately invalid scratch data
    $repoRoot = $sourceRepo
    $Interactive = $true
    $D1InD2 = $false
    $fixtures = @(Get-DemoCandidates | Where-Object Fixture)
    Assert-True ($fixtures.Count -eq 28) 'Expected all 28 public fixture recordings'
    Assert-True (@($fixtures | Where-Object Game -eq d1).Count -eq 18) 'Expected 18 D1 fixture recordings'
    Assert-True (@($fixtures | Where-Object Game -eq d2).Count -eq 10) 'Expected 10 D2 fixture recordings'
    Assert-True (@($fixtures.Path | Select-Object -Unique).Count -eq 28) 'Fixture inventory contains duplicate recordings'
    $repoRoot = Join-Path $fixtureRoot 'repository with spaces'
    foreach ($relative in @('android/regression_demos', 'android/tests/fixtures', 'android/temp_game_logs', 'android/helpers', 'temp', 'game_data')) {
        New-Item -ItemType Directory -Path (Join-Path $repoRoot $relative) -Force | Out-Null
    }
    foreach ($root in (Get-InputDemoFixtureRoots -RepoRoot $sourceRepo)) {
        Copy-Item -LiteralPath $root -Destination (Join-Path $repoRoot 'android/tests/fixtures') -Recurse
    }
    Copy-Item -LiteralPath (Join-Path $sourceRepo 'android/tests/fixtures/demo_import_oracles.json') -Destination (Join-Path $repoRoot 'android/tests/fixtures')
    Copy-Item -LiteralPath (Join-Path $sourceRepo 'game_data/test_data_manifest.json') -Destination (Join-Path $repoRoot 'game_data')
    # Any accidental setup invocation fails locally before downloading or extracting
    Set-Content -LiteralPath (Join-Path $repoRoot 'android/helpers/setup_shareware_test_data.ps1') -Value "throw 'Unexpected public-data setup invocation'"
    foreach ($name in @('old capture', 'new capture')) {
        $path = Join-Path $repoRoot "android/regression_demos/$name.dximdemo"
        Write-MenuTestDemo -Path $path
        (Get-Item -LiteralPath $path).LastWriteTimeUtc = if ($name -eq 'old capture') { [datetime]'2020-01-01' } else { [datetime]'2021-01-01' }
    }
    foreach ($relative in @('temp', 'android/temp_game_logs')) {
        Set-Content -LiteralPath (Join-Path (Join-Path $repoRoot $relative) 'must-not-discover.dximdemo') -Value 'unfinished scratch recording'
    }
    Assert-True ($null -eq (Get-InputDemoFixtureInfo -RepoRoot $repoRoot -Path (Join-Path $repoRoot 'android/tests/fixtures/public_all_weapons/private.dximdemo'))) 'Unlisted files must not inherit fixture metadata'
    $candidates = @(Get-DemoCandidates)
    Assert-True ($candidates.Count -eq 30) 'Default interactive discovery lost old or public recordings'
    Assert-True ($candidates[0].Path.EndsWith('new capture.dximdemo') -and $candidates[1].Path.EndsWith('old capture.dximdemo')) 'Existing recordings must retain newest-first order before public fixtures'
    $paths = @($candidates | Where-Object Fixture | ForEach-Object RelativePath)
    Assert-True (($paths -join '|') -ceq (($paths | Sort-Object) -join '|')) 'Public fixture ordering must be path-stable'
    $D1InD2 = $true
    $imported = @(Get-DemoCandidates -RecordedGame d1)
    Assert-True ($imported.Count -eq 18 -and @($imported | Where-Object { $_.Fixture -and $_.Fixture.NativeOnly }).Count -eq 0) 'Imported inventory must exclude the two native-only D1 grants'
    $D1InD2 = $false
    foreach ($case in @(@('1', 20), @('2', 10), @('3', 18))) {
        $output = Invoke-MenuTestWrapper -Answers @($case[0]) -Options @{ Interactive = $true; ListOnly = $true; RepositoryRoot = $repoRoot }
        Assert-True ([regex]::Matches($output, '(?m)^\[\d+\]').Count -eq $case[1]) "Wrong default inventory for engine $($case[0])"
        Assert-True (-not $output.Contains('must-not-discover')) 'Interactive picker discovered scratch output'
        if ($case[0] -eq '3') {
            Assert-True (-not $output.Contains('d1-bigred') -and -not $output.Contains('d1-scourge')) 'Imported picker offered native-only grants'
            Assert-True ($output.Contains('KNOWN IMPORTED FAULT')) 'Imported diagnostic limitations must be visible'
        }
    }
    foreach ($answers in @(@('q'), @('1', 'q'), @('1', '1', 'q'))) {
        $null = Invoke-MenuTestWrapper -Answers $answers -Options @{ Interactive = $true; RepositoryRoot = $repoRoot; BuildBeforeRun = $true }
        Assert-True (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'temp/input_demo_runtime_wrapper'))) 'Cancellation created a replay sandbox'
    }
    $emptyRoot = Join-Path $fixtureRoot 'empty'
    New-Item -ItemType Directory -Path $emptyRoot | Out-Null
    Assert-Throws { Invoke-MenuTestWrapper -Answers @('1') -Options @{ Interactive = $true; SearchRoot = $emptyRoot } } 'No .dximdemo files are available to replay'
    Assert-Throws { Invoke-MenuTestWrapper -Answers @('1') -Options @{ Interactive = $true; SearchRoot = (Join-Path $fixtureRoot 'missing') } } 'Search root not found:*'
    Assert-Throws { Invoke-MenuTestWrapper -Options @{ DemoPath = (Join-Path $fixtureRoot 'missing.dximdemo') } } 'Demo file not found:*'
    $publicDemo = Join-Path $repoRoot 'android/tests/fixtures/public_all_weapons/d1-bigred.dximdemo'
    Assert-Throws { Invoke-MenuTestWrapper -Options @{ DemoPath = $publicDemo; RepositoryRoot = $repoRoot; DataEdition = 'retail' } } '*UNVERIFIED recording assets*'
    Assert-Throws { Invoke-MenuTestWrapper -Options @{ DemoPath = $publicDemo; RepositoryRoot = $repoRoot; Game = 'd2' } } '*fixture was recorded in d1; select that engine*'
    Assert-Throws { Invoke-MenuTestWrapper -Options @{ DemoPath = $publicDemo; RepositoryRoot = $repoRoot; D1InD2 = $true } } '*qualified only in their native engine*'
    Assert-Throws { Invoke-MenuTestWrapper -Options @{ DemoPath = $publicDemo; RepositoryRoot = $repoRoot; ResolveDataDirOnly = $true; DataEdition = 'shareware' } } '*Prepare the pinned public fixtures explicitly:*setup_shareware_test_data.ps1*'
    foreach ($cancel in @('q', '')) {
        $output = Invoke-MenuTestWrapper -Answers @('1', '1', $cancel) -Options @{ Interactive = $true; DemoPath = $publicDemo; RepositoryRoot = $repoRoot; DataEdition = 'shareware'; AllowUnverifiedAssets = $true }
        Assert-True ($output.Contains('Replay cancelled; no game data was prepared')) 'Public data setup cancellation must be explicit'
        Assert-True ($script:menuTestAnswers.Count -eq 0) 'Public data setup cancellation did not consume the intended prompt'
    }
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'game_data/shareware'))) 'Inventory or cancellation unexpectedly prepared public data'
    Write-Host 'PASS: real picker inventory, ordering, engine filters, isolated roots and offline cancellation'

    # Public JSONL orders keys differently from older recorder output and carries ft
    $fpsCases = @(
        @('public_all_weapons/d1-bigred.dximdemo', 8),
        @('public_d1_fault_probes/d1-mine-blast.dximdemo', 16),
        @('public_level_replays/d1-public-door-lifecycle.dximdemo', 20),
        @('public_level_replays/d2-public-combat.dximdemo', 30),
        @('public_level_replays/d1-public-flight.dximdemo', 60)
    )
    $Runner = 'visual'
    $ExecutablePath = 'not-started'
    foreach ($case in $fpsCases) {
        $path = Join-Path $sourceRepo ('android/tests/fixtures/' + $case[0])
        $mode = Get-LaunchMode -RequestedMode realtime -Path $path -Config @{ MaxFps = 200 }
        Assert-True ($mode.MaxFps -eq $case[1] -and $mode.ExtraArgs.Count -eq 0) "Incorrect real-time frame rate for $($case[0])"
        $selection = Resolve-ReplayRunnerSelection -RequestedRunner visual -ResolvedGame d1 -Header @{ start_mode = 'new_level' } -LaunchMode $mode
        Assert-True (-not $selection.UseHeadlessConsole -and -not $selection.UseNoRender) 'Real-time viewing must use the headed renderer'
        $arguments = @(Get-LaunchArguments -Config @{ TitleArg = '-notitles' } -ResolvedDataDir 'data with spaces' -ResolvedDemoPath $path -ActualResultPath 'result with spaces.json' -LaunchMode $mode -RenderProfileSelection @{ ExtraArgs = @() } -ExitAfterReplay)
        Assert-True ('-window' -in $arguments -and '-inputdemo-exit' -in $arguments -and '-nonicefps' -notin $arguments -and '-inputdemo-norender' -notin $arguments) 'Real-time launch added accelerated or no-present flags'
        Assert-True ($arguments[[array]::IndexOf($arguments, '-maxfps') + 1] -eq [string]$case[1]) 'Launch arguments lost the recorded frame rate'
        Assert-True ('data with spaces' -in $arguments -and 'result with spaces.json' -in $arguments) 'Launch argument arrays split paths containing spaces'
    }
    $legacyArguments = @(Get-LaunchArguments -Config @{ TitleArg = '-notitles' } -ResolvedDataDir 'data' -ResolvedDemoPath 'user recording' -ActualResultPath 'result' -LaunchMode @{ MaxFps = 30; ExtraArgs = @() } -RenderProfileSelection @{ ExtraArgs = @() })
    Assert-True ('-inputdemo-exit' -notin $legacyArguments) 'Existing user recordings must not opt into natural exit automatically'
    $timing = Join-Path $fixtureRoot 'timing.dximdemo'
    @('// comment containing "type":"frame"', '{"type":"other","nested":{"type":"frame"},"ft":1}', '{"ft":8192,"type":"frame"}', '{ "type" : "frame" }', '{"type":"frame","ft":4096}', '{"type":"frame"}') |
        Set-Content -LiteralPath $timing -Encoding utf8
    Assert-True ((Get-DemoApproximateReplayFps -Path $timing -MaxFps 200) -eq 11) 'FPS calculation must weight inherited and changed frame times'
    Assert-True ((Get-DemoApproximateReplayFps -Path $timing -MaxFps 8) -eq 8) 'FPS calculation ignored its upper limit'
    Write-MenuTestDemo -Path $timing
    Assert-True ((Get-DemoApproximateReplayFps -Path $timing -MaxFps 200) -eq 25) 'Frame-free legacy recordings must retain the fallback'
    Write-Host 'PASS: compact JSON and carried frame times preserve 8/16/20/30/60 FPS headed playback'

    $pilotSource = Join-Path $repoRoot 'android/tests/fixtures/public_d1_fan_probes/profile'
    $pilotDestination = Join-Path $fixtureRoot 'isolated pilot'
    Copy-InputDemoComparisonPilot -RepoRoot $repoRoot -Destination $pilotDestination
    foreach ($name in @('d1match.plr', 'd1match.plx')) {
        $sourceHash = (Get-FileHash -LiteralPath (Join-Path $pilotSource $name) -Algorithm SHA256).Hash
        $targetHash = (Get-FileHash -LiteralPath (Join-Path $pilotDestination $name) -Algorithm SHA256).Hash
        Assert-True ($sourceHash -ceq $targetHash) "Comparison pilot was changed while staging: $name"
    }
    Assert-True (@(Get-ChildItem -LiteralPath $pilotDestination -File).Count -eq 2) 'Comparison pilot staging copied unexpected profile files'
    $failedDestination = Join-Path $fixtureRoot 'rejected pilot'
    $extendedPilot = Join-Path $pilotSource 'd1match.plx'
    [IO.File]::AppendAllText($extendedPilot, 'corrupted')
    Assert-Throws { Copy-InputDemoComparisonPilot -RepoRoot $repoRoot -Destination $failedDestination } 'Comparison pilot SHA-256 mismatch:*'
    Assert-True (-not (Test-Path -LiteralPath $failedDestination)) 'Corrupt pilot was partially staged'
    Remove-Item -LiteralPath $extendedPilot
    Assert-Throws { Copy-InputDemoComparisonPilot -RepoRoot $repoRoot -Destination $failedDestination } 'Missing verified comparison pilot:*'
    Assert-True (-not (Test-Path -LiteralPath $failedDestination)) 'Missing pilot was partially staged'
    Write-Host 'PASS: comparison pilot bytes stay pinned and missing/corrupt files fail before staging'

    Set-MenuTestAnswers -Answers @('q')
    $output = (& (Join-Path $androidRoot 'run_all_tests.ps1') -ReplayDemo 6>&1 | Out-String)
    Assert-True ($LASTEXITCODE -eq 0 -and $script:menuTestAnswers.Count -eq 0) 'Suite replay cancellation should exit successfully'
    Assert-True (-not $output.Contains('Extended sampling:')) 'Replay must not fall through into the suite'
} finally {
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host 'PASS: input-demo replay menus and filtering'
