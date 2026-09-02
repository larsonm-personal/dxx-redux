$script:GuidebotSimulationSchema = 'dxx-guidebot-route-simulation-v1'
$script:GuidebotSimulationGeneration = 4
# Keep the fixed timestep and seed synchronized with route_confirmation.h
$script:GuidebotSimulationFixedHz = 60
$script:GuidebotSimulationSeed = 1
$script:GuidebotSimulationStatuses = @(
    'ok',
    'partial',
    'failed',
    'timeout',
    'unsupported',
    'route_mismatch',
    'nondeterministic',
    'not_run',
    'stale'
)

function Get-GuidebotPropertyValue {
    param(
        [AllowNull()][object]$InputObject,
        [Parameter(Mandatory)][string]$Name,
        [AllowNull()][object]$Default = $null
    )

    if ($null -eq $InputObject) { return $Default }
    $property = $InputObject.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) { return $Default }
    return $property.Value
}

function ConvertTo-GuidebotRouteProjection {
    param(
        [Parameter(Mandatory)][object]$Mission,
        [Parameter(Mandatory)][object]$Level
    )

    $steps = foreach ($step in @(Get-GuidebotPropertyValue -InputObject $Level -Name 'route_steps' -Default @())) {
        [ordered]@{
            index = [int](Get-GuidebotPropertyValue -InputObject $step -Name 'index' -Default -1)
            kind = [string](Get-GuidebotPropertyValue -InputObject $step -Name 'kind' -Default '')
            activation_kind = [string](Get-GuidebotPropertyValue -InputObject $step -Name 'activation_kind' -Default '')
            seg = [int](Get-GuidebotPropertyValue -InputObject $step -Name 'seg' -Default -1)
            side = [int](Get-GuidebotPropertyValue -InputObject $step -Name 'side' -Default -1)
            wall = [int](Get-GuidebotPropertyValue -InputObject $step -Name 'wall' -Default -1)
            trigger = [int](Get-GuidebotPropertyValue -InputObject $step -Name 'trigger' -Default -1)
            key = [string](Get-GuidebotPropertyValue -InputObject $step -Name 'key' -Default '')
            key_carrier_objnum = [int](Get-GuidebotPropertyValue -InputObject $step -Name 'key_carrier_objnum' -Default -1)
        }
    }
    return [ordered]@{
        mission_filename = [string](Get-GuidebotPropertyValue -InputObject $Mission -Name 'mission_filename' -Default '')
        target_index = [int](Get-GuidebotPropertyValue -InputObject $Mission -Name 'target_index' -Default 0)
        level_num = [int](Get-GuidebotPropertyValue -InputObject $Level -Name 'level_num' -Default 0)
        secret = [bool](Get-GuidebotPropertyValue -InputObject $Level -Name 'secret' -Default $false)
        level_file = [string](Get-GuidebotPropertyValue -InputObject $Level -Name 'level_file' -Default '')
        route_status = [string](Get-GuidebotPropertyValue -InputObject $Level -Name 'route_status' -Default '')
        route_steps = @($steps)
    }
}

function Get-GuidebotSha256 {
    param([Parameter(Mandatory)][string]$Text)

    $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace('-', '')
    } finally {
        $algorithm.Dispose()
    }
}

function Get-GuidebotRouteInputHash {
    param(
        [Parameter(Mandatory)][object]$Mission,
        [Parameter(Mandatory)][object]$Level
    )

    $projection = ConvertTo-GuidebotRouteProjection -Mission $Mission -Level $Level
    $json = $projection | ConvertTo-Json -Depth 20 -Compress
    return Get-GuidebotSha256 -Text $json
}

function Get-GuidebotExpectedObjectives {
    param([Parameter(Mandatory)][object]$Level)

    return @(
        foreach ($step in @(Get-GuidebotPropertyValue -InputObject $Level -Name 'route_steps' -Default @())) {
            $kind = [string](Get-GuidebotPropertyValue -InputObject $step -Name 'kind' -Default '')
            if ($kind -eq 'start') { continue }
            [pscustomobject][ordered]@{
                route_step_index = [int](Get-GuidebotPropertyValue -InputObject $step -Name 'index' -Default -1)
                kind = $kind
                activation_kind = [string](Get-GuidebotPropertyValue -InputObject $step -Name 'activation_kind' -Default '')
            }
        }
    )
}

function Test-GuidebotObjectiveProjectionMatch {
    param(
        [Parameter(Mandatory)][object]$Level,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Actual,
        [switch]$AllowPrefix
    )

    $expected = @(Get-GuidebotExpectedObjectives -Level $Level)
    $expectedByIndex = @{}
    foreach ($expectedItem in $expected) { $expectedByIndex[[int]$expectedItem.route_step_index] = $expectedItem }
    $seen = @{}
    $nextExpected = 0
    foreach ($actualItem in $Actual) {
        $stepIndex = [int](Get-GuidebotPropertyValue $actualItem 'route_step_index' -1)
        if (-not $expectedByIndex.ContainsKey($stepIndex)) { return $false }
        $expectedItem = $expectedByIndex[$stepIndex]
        if ([string](Get-GuidebotPropertyValue $actualItem 'kind' '') -ne $expectedItem.kind -or
            [string](Get-GuidebotPropertyValue $actualItem 'activation_kind' '') -ne $expectedItem.activation_kind) {
            return $false
        }
        if ($seen.ContainsKey($stepIndex)) { continue }
        if ($nextExpected -ge $expected.Count -or $stepIndex -ne $expected[$nextExpected].route_step_index) {
            return $false
        }
        $seen[$stepIndex] = $true
        $nextExpected++
    }
    return $AllowPrefix -or $nextExpected -eq $expected.Count
}

function ConvertTo-GuidebotRngBoundary {
    param([AllowNull()][object]$Boundary)

    $simulation = Get-GuidebotPropertyValue $Boundary 'simulation'
    return [ordered]@{
        state = [uint32](Get-GuidebotPropertyValue $simulation 'state' 0)
        calls = [uint32](Get-GuidebotPropertyValue $simulation 'calls' 0)
    }
}

function ConvertTo-GuidebotObjectiveName {
    param([Parameter(Mandatory)][object]$Objective)

    $label = ([string](Get-GuidebotPropertyValue $Objective 'label' '')).Trim().ToLowerInvariant()
    if ($label -match '^shoot switch trigger (\d+)$') { return "switch $($Matches[1])" }
    if ($label -match '^fly-through trigger (\d+)$') { return "fly-through $($Matches[1])" }
    if ($label -match '^pass through trigger (\d+)$') { return "pass-through $($Matches[1])" }
    switch ($label) {
        'destroy blastable wall' { return 'blastable wall' }
        'boss robot' { return 'boss' }
        'open hidden door' { return 'hidden door' }
        default {
            if ($label) { return $label }
            return ([string](Get-GuidebotPropertyValue $Objective 'kind' 'objective')).ToLowerInvariant()
        }
    }
}

function ConvertTo-GuidebotLevelSimulationResult {
    param(
        [Parameter(Mandatory)][object]$Mission,
        [Parameter(Mandatory)][object]$Level,
        [Parameter(Mandatory)][object]$EngineResult
    )

    $engineStatus = [string](Get-GuidebotPropertyValue $EngineResult 'status' 'failed')
    $actualObjectives = @(Get-GuidebotPropertyValue $EngineResult 'objectives' @())
    $allowPrefix = $engineStatus -ne 'confirmed'
    $projectionMatches = Test-GuidebotObjectiveProjectionMatch -Level $Level -Actual $actualObjectives -AllowPrefix:$allowPrefix
    $status = switch ($engineStatus) {
        'confirmed' { 'ok' }
        'partial' { 'partial' }
        'timeout' { 'timeout' }
        'unsupported' { 'unsupported' }
        default { 'failed' }
    }
    if (-not $projectionMatches) { $status = 'route_mismatch' }
    $objectives = @(
        foreach ($objective in $actualObjectives) {
            [pscustomobject][ordered]@{
                n = ConvertTo-GuidebotObjectiveName -Objective $objective
                s = [int][Math]::Round(
                    [double](Get-GuidebotPropertyValue $objective 'seconds' 0),
                    0,
                    [MidpointRounding]::AwayFromZero
                )
            }
        }
    )
    $record = [ordered]@{
        level_num = [int](Get-GuidebotPropertyValue $Level 'level_num' 0)
        level_file = [string](Get-GuidebotPropertyValue $Level 'level_file' '')
        route_input_sha256 = Get-GuidebotRouteInputHash -Mission $Mission -Level $Level
        status = $status
        rng_start = ConvertTo-GuidebotRngBoundary (Get-GuidebotPropertyValue $EngineResult 'rng_start')
        objectives = $objectives
        total_frames = [uint32](Get-GuidebotPropertyValue $EngineResult 'frames' 0)
        rng_end = ConvertTo-GuidebotRngBoundary (Get-GuidebotPropertyValue $EngineResult 'rng_end')
    }
    if ($status -ne 'ok') {
        $problem = if (-not $projectionMatches) {
            'live objective sequence differs from route input'
        } else {
            [string](Get-GuidebotPropertyValue $EngineResult 'problem' $engineStatus)
        }
        $record.problem = $problem
    }
    return [pscustomobject]$record
}

function Get-GuidebotMissionAggregateStatus {
    param([Parameter(Mandatory)][object[]]$Levels)

    if ($Levels.Count -eq 0) { return 'not_run' }
    $statuses = @($Levels | ForEach-Object { [string]$_.status })
    if (@($statuses | Where-Object { $_ -ne 'ok' }).Count -eq 0) { return 'ok' }
    if (@($statuses | Where-Object { $_ -in @('ok', 'partial') }).Count -gt 0) { return 'partial' }
    if (@($statuses | Where-Object { $_ -eq 'not_run' }).Count -eq $statuses.Count) { return 'not_run' }
    if (@($statuses | Where-Object { $_ -eq 'stale' }).Count -eq $statuses.Count) { return 'stale' }
    return 'failed'
}

function New-GuidebotMissionSimulationRecord {
    param(
        [Parameter(Mandatory)][object]$Mission,
        [Parameter(Mandatory)][object[]]$Levels
    )

    $counts = [ordered]@{}
    foreach ($status in $script:GuidebotSimulationStatuses) {
        $count = @($Levels | Where-Object { $_.status -eq $status }).Count
        if ($count -gt 0) { $counts[$status] = $count }
    }
    return [pscustomobject][ordered]@{
        schema = $script:GuidebotSimulationSchema
        mission_filename = [string](Get-GuidebotPropertyValue $Mission 'mission_filename' '')
        target_index = [int](Get-GuidebotPropertyValue $Mission 'target_index' 0)
        generation = $script:GuidebotSimulationGeneration
        fixed_hz = $script:GuidebotSimulationFixedHz
        seed = $script:GuidebotSimulationSeed
        status = Get-GuidebotMissionAggregateStatus -Levels $Levels
        level_counts = [pscustomobject]$counts
        levels = @($Levels)
    }
}

function ConvertTo-GuidebotNormalizedJsonText {
    param([Parameter(Mandatory)][object]$Value)

    return (($Value | ConvertTo-Json -Depth 30) -replace "`r`n", "`n") + "`n"
}

function Write-GuidebotSimulationJson {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][object]$Value
    )

    $parent = Split-Path -Parent $Path
    if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    $temporaryPath = "$Path.$([Guid]::NewGuid().ToString('N')).tmp"
    try {
        [IO.File]::WriteAllText(
            $temporaryPath,
            (ConvertTo-GuidebotNormalizedJsonText -Value $Value),
            [Text.UTF8Encoding]::new($false)
        )
        Move-Item -LiteralPath $temporaryPath -Destination $Path -Force
    } finally {
        Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue
    }
}

function Test-GuidebotMissionSimulationRecord {
    param(
        [Parameter(Mandatory)][object]$Record,
        [switch]$ThrowOnError
    )

    $errors = [Collections.Generic.List[string]]::new()
    if ([string](Get-GuidebotPropertyValue $Record 'schema' '') -ne $script:GuidebotSimulationSchema) {
        $errors.Add('invalid schema')
    }
    $levels = @(Get-GuidebotPropertyValue $Record 'levels' @())
    $identities = @{}
    foreach ($level in $levels) {
        $identity = "$(Get-GuidebotPropertyValue $level 'level_num' 0)|$(Get-GuidebotPropertyValue $level 'level_file' '')"
        if ($identities.ContainsKey($identity)) { $errors.Add("duplicate level identity $identity") }
        $identities[$identity] = $true
        $status = [string](Get-GuidebotPropertyValue $level 'status' '')
        if ($status -notin $script:GuidebotSimulationStatuses) { $errors.Add("unknown level status $status") }
        $previous = -1.0
        foreach ($objective in @(Get-GuidebotPropertyValue $level 'objectives' @())) {
            $name = [string](Get-GuidebotPropertyValue $objective 'n' '')
            $seconds = Get-GuidebotPropertyValue $objective 's' -1
            if (-not $name) { $errors.Add("empty objective name for $identity") }
            if ([double]$seconds -ne [Math]::Truncate([double]$seconds)) {
                $errors.Add("noninteger objective seconds for $identity")
            }
            if ([double]$seconds -lt $previous) { $errors.Add("nonmonotonic objective seconds for $identity") }
            $previous = [double]$seconds
        }
    }
    $expectedAggregate = Get-GuidebotMissionAggregateStatus -Levels $levels
    if ([string](Get-GuidebotPropertyValue $Record 'status' '') -ne $expectedAggregate) {
        $errors.Add('mission aggregate status does not match levels')
    }
    if ($ThrowOnError -and $errors.Count -gt 0) { throw ($errors -join '; ') }
    return [pscustomobject]@{ Valid = $errors.Count -eq 0; Errors = @($errors) }
}
