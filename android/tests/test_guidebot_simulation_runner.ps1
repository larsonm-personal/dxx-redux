#!/usr/bin/env pwsh

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runner = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$tempRoot = Join-Path $repoRoot 'android\temp\guidebot_simulation_runner_test'
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

function Invoke-DryRun {
    param(
        [string]$Name,
        [string[]]$MissionJson,
        [int[]]$Level,
        [double]$SampleFraction = 1,
        [int]$SampleSeed = 0,
        [string]$SampleStatePath,
        [switch]$RoutingDevelopmentSet,
        [ValidateSet('Headless', 'Headed', 'Desktop')][string]$Mode = 'Headless'
    )
    $output = Join-Path $tempRoot "$Name.json"
    $parameters = @{ DryRun = $true; NoBuild = $true; DryRunJsonOut = $output; Mode = $Mode }
    if ($MissionJson) { $parameters.MissionJson = $MissionJson }
    if ($Level) { $parameters.Level = $Level }
    if ($SampleFraction -lt 1) { $parameters.SampleFraction = $SampleFraction; $parameters.SampleSeed = $SampleSeed }
    if ($SampleStatePath) { $parameters.SampleStatePath = $SampleStatePath }
    if ($RoutingDevelopmentSet) { $parameters.RoutingDevelopmentSet = $true }
    & $runner @parameters | Out-Null
    return @(Get-Content -LiteralPath $output -Raw | ConvertFrom-Json)
}

try {
    $runnerSource = Get-Content -LiteralPath $runner -Raw
    $runnerAst = [Management.Automation.Language.Parser]::ParseInput($runnerSource, [ref]$null, [ref]$null)
    $stageFunction = $runnerAst.Find({
            param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Copy-GuidebotFlatStage'
        }, $true)
    . ([scriptblock]::Create($stageFunction.Extent.Text))
    $stageSource = Join-Path $tempRoot 'stage_source'
    $stageDestination = Join-Path $tempRoot 'stage_destination'
    New-Item -ItemType Directory -Path $stageSource -Force | Out-Null
    $descriptorText = "name = fixture`nnum_levels = 1`nfixture.rdl"
    [IO.File]::WriteAllText((Join-Path $stageSource 'fixture.msn'), $descriptorText + [char]0x1a)
    [IO.File]::WriteAllBytes((Join-Path $stageSource 'fixture.hog'), [byte[]]@(68, 72, 70, 26))
    Copy-GuidebotFlatStage -Source $stageSource -Destination $stageDestination
    if ([IO.File]::ReadAllText((Join-Path $stageDestination 'fixture.msn')) -cne $descriptorText -or
        [IO.File]::ReadAllBytes((Join-Path $stageDestination 'fixture.hog'))[3] -ne 26) {
        throw 'DOS EOF normalization must apply to descriptors only'
    }
    if ($runnerSource -notmatch "headless_process_pool\.ps1" -or
        $runnerSource -notmatch '(?s)if \(\$Mode -eq ''Headless''\).*?Invoke-HeadlessProcessPool' -or
        $runnerSource -match '(?s)function Invoke-GuidebotDesktopLevel.*?Start-Process.*?-NoNewWindow') {
        throw 'Headless runs must use the hidden process pool while Desktop runs retain their visible game window'
    }
    if ($runnerSource -notmatch '\[int\]\$Repeat\s*=\s*1') {
        throw 'Corpus runs must default to one deterministic execution per level'
    }
    $processPool = $runnerSource.IndexOf('Invoke-HeadlessProcessPool -Tasks')
    $incrementalWrite = $runnerSource.IndexOf('Publish-GuidebotResult -Item $task.Item', $processPool)
    $finalWriteLoop = $runnerSource.IndexOf('foreach ($file in $files) {', $incrementalWrite)
    if ($processPool -lt 0 -or $incrementalWrite -lt $processPool -or $finalWriteLoop -lt $incrementalWrite) {
        throw 'Each completed level must be published before final corpus aggregation'
    }
    $filtered = @(Invoke-DryRun -Name filtered -MissionJson Counterstrike.json -Level 1, 3)
    if ($filtered.Count -ne 2 -or ($filtered.identity -join ',') -notmatch '\|1\|' -or
        ($filtered.identity -join ',') -notmatch '\|3\|') {
        throw 'Mission and level dry-run filtering is incorrect'
    }
    $desktop = @(Invoke-DryRun -Name desktop -MissionJson Counterstrike.json -Level 1 -Mode Desktop)
    $expectedDesktop = @($filtered | Where-Object { $_.identity -match '\|1\|' })
    if (($desktop.identity -join ',') -cne ($expectedDesktop.identity -join ',')) {
        throw 'Desktop dry-run discovery differs from the canonical runner'
    }
    $multiDescriptor = @(Invoke-DryRun -Name multi -MissionJson '-MOON-.json')
    if ($multiDescriptor.Count -ne 14 -or @($multiDescriptor.identity | Select-Object -Unique).Count -ne 14) {
        throw 'Multi-descriptor mission work item expansion is incorrect'
    }
    if (@($multiDescriptor | Where-Object { $_.identity -notmatch '\|0\|' -or $_.engine_level -ne 1 }).Count) {
        throw 'Level-zero metadata must retain its identity while launching engine level 1'
    }
    $routingSet = @(Invoke-DryRun -Name routing_set -RoutingDevelopmentSet)
    $routingFiles = @($routingSet.identity | ForEach-Object { $_.Split('|')[0] } | Select-Object -Unique)
    if (($routingFiles -join ',') -cne 'af_d1_beta.json,Bahagad.json,bitesize.json,castaway_redux.json,CD - Descent II - The Vertigo Series (USA).json,Counterstrike.json,descent_maximum_fixed.json,diehard.json,EAF.json,EAF2.json,Entropy2.json,FirstStrike.json,Lostlvls.json,Mandrill.json,Obsidian.json,plutonia.json,TEW.json,Vignettes.json') {
        throw "Routing development set selected unexpected mission files: $($routingFiles -join ', ')"
    }
    $tew = @($routingSet | Where-Object { $_.identity -like 'TEW.json|*' })
    $plutonia = @($routingSet | Where-Object { $_.identity -like 'plutonia.json|*' })
    $vertigo = @($routingSet | Where-Object { $_.identity -like 'CD - Descent II - The Vertigo Series (USA).json|*' })
    $vignettes = @($routingSet | Where-Object { $_.identity -like 'Vignettes.json|*' })
    $entropy2 = @($routingSet | Where-Object { $_.identity -like 'Entropy2.json|*' })
    $maximum = @($routingSet | Where-Object { $_.identity -like 'descent_maximum_fixed.json|*' })
    $af = @($routingSet | Where-Object { $_.identity -like 'af_d1_beta.json|*' })
    $mandrill = @($routingSet | Where-Object { $_.identity -like 'Mandrill.json|*' })
    $bitesize = @($routingSet | Where-Object { $_.identity -like 'bitesize.json|*' })
    if ($bitesize.Count -ne 31) { throw 'Bitesize must include all 31 levels' }
    if ($maximum.Count -ne 36 -or $af.Count -ne 10 -or $mandrill.Count -ne 7) {
        throw 'The expanded set must include both Descent Maximum variants, ten AF levels, and seven Mandrill levels'
    }
    if ($routingSet.Count -ne 362 -or $tew.Count -ne 32 -or $plutonia.Count -ne 32 -or $vertigo.Count -ne 24 -or $vignettes.Count -ne 27 -or $entropy2.Count -ne 6) {
        throw 'The eighteen-mission set must include 362 total levels'
    }
    foreach ($entry in @(@{ Name = 'Lostlvls'; Count = 25 }, @{ Name = 'EAF2'; Count = 10 }, @{ Name = 'EAF'; Count = 5 }, @{ Name = 'Bahagad'; Count = 10 }, @{ Name = 'diehard'; Count = 19 })) {
        if (@($routingSet | Where-Object { $_.identity -like "$($entry.Name).json|*" }).Count -ne $entry.Count) {
            throw "Expanded set omitted levels from $($entry.Name)"
        }
    }
    $firstStrike = @($routingSet | Where-Object { $_.identity -like 'FirstStrike.json|*' })
    if ($firstStrike.Count -ne 30 -or @($firstStrike | Where-Object engine_mode -ne 'd1_in_d2').Count) {
        throw 'Canonical First Strike must expose all 30 native D1 levels through the D2 simulation engine'
    }
    $customD1 = @(Invoke-DryRun -Name custom_d1 -MissionJson trainng.json -Level 1)
    if ($customD1.Count -ne 1 -or $customD1[0].engine_mode -ne 'd1_in_d2' -or
        $customD1[0].identity -notmatch '\.rdl$') {
        throw 'Custom D1 archives must be discovered as D1-in-D2 work items'
    }
    $sampleA = @(Invoke-DryRun -Name sample_a -MissionJson Counterstrike.json -SampleFraction 0.25 `
            -SampleSeed 717 -SampleStatePath (Join-Path $tempRoot 'sample_a_state.json'))
    $sampleB = @(Invoke-DryRun -Name sample_b -MissionJson Counterstrike.json -SampleFraction 0.25 `
            -SampleSeed 717 -SampleStatePath (Join-Path $tempRoot 'sample_b_state.json'))
    if ($sampleA.Count -eq 0 -or ($sampleA.identity -join ',') -cne ($sampleB.identity -join ',')) {
        throw 'Hash-ring dry-run selection is not deterministic from equal fresh state'
    }

    $failureRoot = Join-Path $tempRoot 'infrastructure_failure'
    $pwsh = (Get-Process -Id $PID).Path
    $failureLog = Join-Path $tempRoot 'infrastructure_failure.log'
    $failureArguments = @(
        '-NoProfile', '-File', $runner,
        '-Mode', 'Headless', '-MissionJson', 'Counterstrike.json', '-Level', '1',
        '-Repeat', '1', '-NoBuild', '-OutputRoot', $failureRoot,
        '-HeadlessExecutable', $pwsh
    ) | ForEach-Object {
        $argument = [string]$_
        if ($argument -match '[\s"]') { '"' + $argument.Replace('"', '\"') + '"' } else { $argument }
    }
    $failureProcess = Start-Process -FilePath $pwsh -ArgumentList ($failureArguments -join ' ') `
        -Wait -PassThru -NoNewWindow -RedirectStandardOutput $failureLog -RedirectStandardError "$failureLog.stderr"
    if ($failureProcess.ExitCode -eq 0) {
        throw 'Injected route engine failure unexpectedly succeeded'
    }
    $failureOutput = Get-Content -LiteralPath (Join-Path $failureRoot 'results\Counterstrike.simulation.json') `
        -Raw | ConvertFrom-Json
    $failedLevel = @($failureOutput.levels | Where-Object { $_.level_num -eq 1 })
    if ($failedLevel.Count -ne 1 -or $failedLevel[0].status -ne 'infrastructure_error' -or
        -not [string]$failedLevel[0].problem) {
        throw 'Infrastructure failure was not published in the level regression result'
    }
    $unsupportedMetadata = Join-Path $tempRoot 'unsupported_metadata'
    New-Item -ItemType Directory -Path $unsupportedMetadata -Force | Out-Null
    foreach ($problem in @('unsupported level version 23 (maximum 8)', 'invalid level header')) {
        $mission = Get-Content -LiteralPath (Join-Path $repoRoot 'game_data/mission_files/Counterstrike.json') -Raw | ConvertFrom-Json
        $mission.levels = @($mission.levels[0])
        $mission.levels[0] | Add-Member -NotePropertyName route_problem -NotePropertyValue $problem -Force
        $mission.levels[0] | Add-Member -NotePropertyName status -NotePropertyValue failed -Force
        [IO.File]::WriteAllText((Join-Path $unsupportedMetadata 'unsupported.json'), ($mission | ConvertTo-Json -Depth 100))
        $unsupportedRoot = Join-Path $tempRoot ('unsupported-' + [guid]::NewGuid().ToString('N'))
        & $runner -NoBuild -MissionMetadataRoot $unsupportedMetadata -OutputRoot $unsupportedRoot -HeadlessExecutable $pwsh | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Known unreadable level launched the route engine' }
        $unsupported = Get-Content -LiteralPath (Join-Path $unsupportedRoot 'results/unsupported.simulation.json') -Raw | ConvertFrom-Json
        if ($unsupported.levels[0].status -ne 'unsupported' -or $unsupported.levels[0].problem -ne $problem) {
            throw 'Unsupported level diagnostic was not retained in simulation output'
        }
    }
    Write-Host 'GuideBot simulation runner discovery and sampling passed'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}
