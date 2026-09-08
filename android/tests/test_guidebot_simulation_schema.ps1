$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repoRoot 'android\helpers\guidebot_simulation_regression.ps1')

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$mission = [pscustomobject]@{
    mission_filename = 'fixture'
    target_index = 0
    levels = @(
        [pscustomobject]@{
            level_num = 1
            level_file = 'fixture-1.rl2'
            secret = $false
            route_status = 'ok'
            route_steps = @(
                [pscustomobject]@{ index = 0; kind = 'start'; activation_kind = 'none'; seg = 0 }
                [pscustomobject]@{ index = 1; kind = 'key'; activation_kind = 'pickup_key'; seg = 2; key = 'red' }
                [pscustomobject]@{ index = 2; kind = 'exit'; activation_kind = 'enter_exit'; seg = 3; side = 1; wall = 4; trigger = 0 }
            )
        }
    )
}
$engine = [pscustomobject]@{
    status = 'confirmed'
    frames = 120
    rng_start = [pscustomobject]@{
        simulation = [pscustomobject]@{ state = 1; calls = 0 }
        effects = [pscustomobject]@{ state = 1; calls = 0 }
    }
    rng_end = [pscustomobject]@{
        simulation = [pscustomobject]@{ state = 42; calls = 5 }
        effects = [pscustomobject]@{ state = 99; calls = 2 }
    }
    objectives = @(
        [pscustomobject]@{ route_step_index = 1; kind = 'key'; activation_kind = 'pickup_key'; label = 'red key'; seconds = 1.49 }
        [pscustomobject]@{ route_step_index = 2; kind = 'exit'; activation_kind = 'enter_exit'; label = 'Exit'; seconds = 2.5 }
    )
}

$hash1 = Get-GuidebotRouteInputHash -Mission $mission -Level $mission.levels[0]
$hash2 = Get-GuidebotRouteInputHash -Mission $mission -Level $mission.levels[0]
Assert-True ($hash1 -eq $hash2 -and $hash1.Length -eq 64) 'route projection hash is not stable'

$levelResult = ConvertTo-GuidebotLevelSimulationResult -Mission $mission -Level $mission.levels[0] -EngineResult $engine
Assert-True ($levelResult.status -eq 'ok') 'confirmed matching route did not normalize to ok'
Assert-True ($levelResult.rng_end.state -eq 42 -and $null -eq $levelResult.rng_end.PSObject.Properties['effects']) `
    'simulation-only RNG boundary was not preserved'
Assert-True ($levelResult.objectives[0].n -eq 'red key' -and $levelResult.objectives[0].s -eq 1) `
    'key objective was not compacted and rounded'
Assert-True ($levelResult.objectives[1].n -eq 'exit' -and $levelResult.objectives[1].s -eq 3) `
    'exit objective was not compacted and rounded'
$switchName = ConvertTo-GuidebotObjectiveName -Objective ([pscustomobject]@{
        kind = 'trigger'; label = 'Shoot switch trigger 22'
    })
Assert-True ($switchName -eq 'switch 22') 'switch objective name was not shortened'

$record = New-GuidebotMissionSimulationRecord -Mission $mission -Levels @($levelResult)
$validation = Test-GuidebotMissionSimulationRecord -Record $record
Assert-True $validation.Valid "valid simulation record was rejected: $($validation.Errors -join '; ')"
Assert-True ($record.status -eq 'ok' -and $record.level_counts.ok -eq 1) 'mission summary is incorrect'
$d1Mission = $mission.PSObject.Copy()
$d1Mission | Add-Member -NotePropertyName game -NotePropertyValue 'd1'
$d1Record = New-GuidebotMissionSimulationRecord -Mission $d1Mission -Levels @($levelResult)
Assert-True ($d1Record.engine_mode -eq 'd1_in_d2') 'D1 simulation output must identify the D2 engine mode'

$mismatchEngine = $engine.PSObject.Copy()
$mismatchEngine.objectives = @(
    [pscustomobject]@{ route_step_index = 1; kind = 'reactor'; activation_kind = 'destroy_reactor'; label = 'Reactor'; seconds = 1.0 }
)
$mismatch = ConvertTo-GuidebotLevelSimulationResult -Mission $mission -Level $mission.levels[0] -EngineResult $mismatchEngine
Assert-True ($mismatch.status -eq 'route_mismatch') 'objective drift was not classified as route_mismatch'

$restorerEngine = $engine.PSObject.Copy()
$restorerEngine.objectives = @(
    [pscustomobject]@{ route_step_index = 1; kind = 'key'; activation_kind = 'pickup_key'; label = 'red key'; seconds = 1.0 }
    [pscustomobject]@{ route_step_index = 1; kind = 'key'; activation_kind = 'pickup_key'; label = 'red key'; seconds = 1.5 }
    [pscustomobject]@{ route_step_index = 2; kind = 'exit'; activation_kind = 'enter_exit'; label = 'Exit'; seconds = 2.0 }
)
$restorer = ConvertTo-GuidebotLevelSimulationResult -Mission $mission -Level $mission.levels[0] -EngineResult $restorerEngine
Assert-True ($restorer.status -eq 'ok') 'a valid repeated restorer objective was rejected'

# A live switch restoration may precede, but must never replace, the planned switch
$switchLevel = [pscustomobject]@{
    route_steps = @([pscustomobject]@{
        index = 1; kind = 'trigger'; activation_kind = 'shoot_switch'; wall = 81
    })
}
$recoveryObjective = [pscustomobject]@{
    route_step_index = 1; kind = 'trigger'; activation_kind = 'fly_through_trigger'; restores_switch_wall = 81
}
$switchObjective = [pscustomobject]@{
    route_step_index = 1; kind = 'trigger'; activation_kind = 'shoot_switch'
}
Assert-True (Test-GuidebotObjectiveProjectionMatch -Level $switchLevel -Actual @($recoveryObjective, $switchObjective)) `
    'certified restoration followed by its planned switch was rejected'
Assert-True (-not (Test-GuidebotObjectiveProjectionMatch -Level $switchLevel -Actual @($recoveryObjective))) `
    'restoration incorrectly completed the planned switch'
$wrongRecovery = $recoveryObjective.PSObject.Copy()
$wrongRecovery.restores_switch_wall = 82
Assert-True (-not (Test-GuidebotObjectiveProjectionMatch -Level $switchLevel -Actual @($wrongRecovery, $switchObjective))) `
    'restoration of an unrelated wall was accepted'
Assert-True (-not (Test-GuidebotObjectiveProjectionMatch -Level $switchLevel -Actual @($switchObjective, $recoveryObjective))) `
    'out-of-order restoration was accepted'

$emptyFailureEngine = $engine.PSObject.Copy()
$emptyFailureEngine.status = 'timeout'
$emptyFailureEngine.objectives = @()
$emptyFailureEngine | Add-Member -NotePropertyName problem -NotePropertyValue 'no progress'
$emptyFailure = ConvertTo-GuidebotLevelSimulationResult -Mission $mission -Level $mission.levels[0] -EngineResult $emptyFailureEngine
Assert-True ($emptyFailure.status -eq 'timeout') 'an empty controlled failure did not normalize to timeout'

$partialRecord = New-GuidebotMissionSimulationRecord -Mission $mission -Levels @(
    $levelResult,
    [pscustomobject]@{
        level_num = 2
        level_file = 'fixture-2.rl2'
        route_input_sha256 = ('0' * 64)
        status = 'not_run'
        rng_start = $levelResult.rng_start
        objectives = @()
        total_frames = 0
        rng_end = $levelResult.rng_end
    }
)
Assert-True ($partialRecord.status -eq 'partial') 'mixed mission summary did not normalize to partial'

$json1 = ConvertTo-GuidebotNormalizedJsonText -Value $record
$json2 = ConvertTo-GuidebotNormalizedJsonText -Value $record
Assert-True ($json1 -ceq $json2) 'normalized JSON is not byte-stable'
Assert-True ($json1 -notmatch '"effects"' -and $json1 -notmatch 'objective_seconds') `
    'retired RNG or objective fields remain in normalized JSON'
Assert-True ($json1 -match '"n": "red key"' -and $json1 -match '"s": 1') `
    'compact named objective fields are missing from normalized JSON'

$arrayJson1 = ConvertTo-GuidebotNormalizedJsonText -Value @($record, $partialRecord)
$arrayJson2 = ConvertTo-GuidebotNormalizedJsonText -Value @($record, $partialRecord)
Assert-True ($arrayJson1 -ceq $arrayJson2 -and $arrayJson1.TrimStart().StartsWith('[')) `
    'multi-mission array output is not byte-stable'

$invalidRecord = $record.PSObject.Copy()
$invalidRecord.levels = @($levelResult, $levelResult)
$invalidRecord.status = 'partial'
$invalidValidation = Test-GuidebotMissionSimulationRecord -Record $invalidRecord
Assert-True (-not $invalidValidation.Valid -and ($invalidValidation.Errors -join ' ') -match 'duplicate') `
    'duplicate level identities were not rejected'

Assert-True ((Get-GuidebotMissionAggregateStatus -Levels @()) -eq 'not_run') `
    'empty level arrays were rejected by the aggregate helper'
$emptyMission = [pscustomobject]@{
    game = 'd1'
    mission_filename = 'empty.msn'
    target_index = 0
    levels = @()
    status = 'failed'
    problems = @('could not mount HOG: unsupported')
}
$emptyRecord = New-GuidebotMissionSimulationRecord -Mission $emptyMission -Levels @()
Assert-True ($emptyRecord.status -eq 'failed' -and $emptyRecord.levels.Count -eq 0) `
    'empty metadata must not become a successful simulation'
Assert-True ($emptyRecord.problem -match 'could not mount HOG') 'metadata failure detail was lost'
Assert-True ((Test-GuidebotMissionSimulationRecord -Record $emptyRecord).Valid) `
    'empty failed mission record did not validate'
$emptyRoundTrip = ConvertTo-GuidebotNormalizedJsonText -Value $emptyRecord | ConvertFrom-Json
Assert-True ($emptyRoundTrip.levels -is [array] -and $emptyRoundTrip.levels.Count -eq 0) `
    'empty levels did not serialize as an array'
$emptyMission.problems = @()
Assert-True ((New-GuidebotMissionSimulationRecord -Mission $emptyMission -Levels @()).status -eq 'failed') `
    'empty metadata without an error must not report success'

# Exercise the actual finalization writer without launching any engine processes
$runnerPath = Join-Path $repoRoot 'android\helpers\regenerate_all_guidebot_simulations.ps1'
$runnerAst = [Management.Automation.Language.Parser]::ParseFile($runnerPath, [ref]$null, [ref]$null)
foreach ($functionName in @('Get-GuidebotMissionEntries', 'Get-ExistingGuidebotLevelMap', 'Write-GuidebotSimulationFile', 'New-GuidebotInfrastructureErrorResult')) {
    $definition = $runnerAst.Find({
            param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $functionName
        }, $true)
    . ([scriptblock]::Create($definition.Extent.Text))
}
$missionRoot = Join-Path $repoRoot "android\temp\empty_simulation_$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Path $missionRoot | Out-Null
$Mode = 'Headless'
foreach ($arrayRoot in @($false, $true)) {
    $metadataPath = Join-Path $missionRoot "$arrayRoot.json"
    $outputPath = Join-Path $missionRoot "$arrayRoot.simulation.json"
    $value = if ($arrayRoot) { , @($emptyMission, $emptyMission) } else { $emptyMission }
    Write-GuidebotSimulationJson -Path $metadataPath -Value $value
    Write-GuidebotSimulationFile -MetadataFile (Get-Item -LiteralPath $metadataPath) `
        -ResultsByIdentity @{} -Destination $outputPath
    $written = @(Get-Content -LiteralPath $outputPath -Raw | ConvertFrom-Json)
    Assert-True ($written.Count -eq $(if ($arrayRoot) { 2 } else { 1 })) 'finalization lost empty mission entries'
    foreach ($entry in $written) {
        Assert-True ($entry.status -eq 'failed' -and $entry.levels.Count -eq 0) 'finalization misreported empty metadata'
    }
}

$failureCases = @(
    @{ Text = 'Route engine infrastructure failure for fixture, exit -1073741819, log={0}'; Problem = 'Route engine infrastructure failure'; Exit = -1073741819 },
    @{ Text = 'Desktop route infrastructure failure for fixture, exit 1, log={0}'; Problem = 'Route engine infrastructure failure'; Exit = 1 },
    @{ Text = 'Route engine process timeout after 180 seconds for fixture, log={0}'; Problem = 'Route engine process timeout'; Timeout = 180 },
    @{ Text = 'Desktop route process timeout after 300 seconds for fixture, log={0}'; Problem = 'Route engine process timeout'; Timeout = 300 },
    @{ Text = 'Route engine could not start for fixture: missing {0}'; Problem = 'Route engine could not start' },
    @{ Text = 'Route engine result could not be read for fixture: invalid JSON in {0}'; Problem = 'Route engine result could not be read' },
    @{ Text = 'Could not stage mission at {0}'; Problem = 'Simulation setup or execution failed' }
)
foreach ($case in $failureCases) {
    $firstFailure = New-GuidebotInfrastructureErrorResult -Mission $mission -LevelRecord $mission.levels[0] `
        -Problem ($case.Text -f 'C:\workspace one\temp\20260904_174756\result.log')
    $secondFailure = New-GuidebotInfrastructureErrorResult -Mission $mission -LevelRecord $mission.levels[0] `
        -Problem ($case.Text -f '/different/workspace/temp/20260905_010000/result.log')
    Assert-True ((ConvertTo-GuidebotNormalizedJsonText $firstFailure) -ceq (ConvertTo-GuidebotNormalizedJsonText $secondFailure)) `
        'run-specific diagnostics changed the regression record'
    Assert-True ($firstFailure.problem -eq $case.Problem) 'failure category was lost'
    if ($case.ContainsKey('Exit')) { Assert-True ($firstFailure.exit_code -eq $case.Exit) 'native exit code was lost' }
    if ($case.ContainsKey('Timeout')) { Assert-True ($firstFailure.timeout_seconds -eq $case.Timeout) 'timeout budget was lost' }
}

Write-Host 'GuideBot simulation schema tests passed'
