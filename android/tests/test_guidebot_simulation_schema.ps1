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

Write-Host 'GuideBot simulation schema tests passed'
