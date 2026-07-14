#!/usr/bin/env pwsh
param(
    [switch]$Update
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$metadataDir = Join-Path $repoRoot "game_data\mission_files"
$baselinePath = Join-Path $PSScriptRoot "fixtures\mission_route_baseline.json"

function ConvertTo-RoundedRouteDistance {
    param($Value)

    if ($null -eq $Value) {
        return $null
    }
    return [Math]::Round([double]$Value, 6, [MidpointRounding]::AwayFromZero)
}

function ConvertTo-RouteStepProjection {
    param([Parameter(Mandatory = $true)]$Step)

    $opens = @(
        @($Step.opens) | ForEach-Object {
            [ordered]@{
                seg = $_.seg
                side = $_.side
                wall = $_.wall
            }
        }
    )
    $projection = [ordered]@{
        index = $Step.index
        kind = $Step.kind
        activation_kind = $Step.activation_kind
        label = $Step.label
        seg = $Step.seg
        side = $Step.side
        wall = $Step.wall
        distance = ConvertTo-RoundedRouteDistance -Value $Step.distance
        key = $Step.key
        trigger = $Step.trigger
        trigger_type_id = $Step.trigger_type_id
        trigger_type = $Step.trigger_type
        opens = $opens
    }
    if ($null -ne $Step.key_carrier_objnum) {
        $projection.key_carrier_objnum = $Step.key_carrier_objnum
    }
    return $projection
}

function Get-Sha256Text {
    param([Parameter(Mandatory = $true)][string]$Text)

    $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $hash = $sha.ComputeHash($bytes)
        return ([BitConverter]::ToString($hash)).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Get-CurrentRouteManifest {
    $records = [Collections.Generic.List[object]]::new()

    Get-ChildItem -LiteralPath $metadataDir -Filter "*.json" |
        Where-Object { $_.Name -notlike "*.tracklist.json" } |
        Sort-Object Name |
        ForEach-Object {
            $metadataFile = $_
            $missions = @(Get-Content -LiteralPath $metadataFile.FullName -Raw | ConvertFrom-Json)
            for ($missionIndex = 0; $missionIndex -lt $missions.Count; $missionIndex++) {
                $mission = $missions[$missionIndex]
                foreach ($level in @($mission.levels)) {
                    $status = if ([string]::IsNullOrWhiteSpace([string]$level.route_status)) {
                        "missing"
                    } else {
                        [string]$level.route_status
                    }
                    $steps = @(
                        @($level.route_steps) | ForEach-Object {
                            ConvertTo-RouteStepProjection -Step $_
                        }
                    )
                    $projection = [ordered]@{
                        game = $mission.game
                        mission_filename = $mission.mission_filename
                        level_num = $level.level_num
                        level_file = $level.level_file
                        route_status = $level.route_status
                        route_problem = $level.route_problem
                        route_note = $level.route_note
                        travel_distance = ConvertTo-RoundedRouteDistance -Value $level.travel_distance
                        travel_time_seconds = $level.travel_time_seconds
                        route_steps = $steps
                    }
                    $projectionJson = $projection | ConvertTo-Json -Compress -Depth 12
                    $records.Add([pscustomobject][ordered]@{
                            key = "$($metadataFile.Name)|$missionIndex|$($level.level_num)|$($level.level_file)"
                            hash = Get-Sha256Text -Text $projectionJson
                            status = $status
                            step_count = $steps.Count
                        })
                }
            }
        }

    $orderedRecords = @($records | Sort-Object key)
    $statusCounts = [ordered]@{}
    foreach ($group in @($orderedRecords | Group-Object status | Sort-Object Name)) {
        $statusCounts[$group.Name] = $group.Count
    }
    return [ordered]@{
        schema = 1
        record_count = $orderedRecords.Count
        status_counts = $statusCounts
        records = $orderedRecords
    }
}

$current = Get-CurrentRouteManifest
if ($Update) {
    $baselineDir = Split-Path -Parent $baselinePath
    [IO.Directory]::CreateDirectory($baselineDir) | Out-Null
    $json = $current | ConvertTo-Json -Depth 8
    $utf8NoBom = [Text.UTF8Encoding]::new($false)
    [IO.File]::WriteAllText($baselinePath, "$json`n", $utf8NoBom)
    Write-Host "Updated route corpus baseline with $($current.record_count) levels"
    exit 0
}

if (-not (Test-Path -LiteralPath $baselinePath)) {
    throw "Route corpus baseline is missing; run this script with -Update and review the generated file"
}

$baseline = Get-Content -LiteralPath $baselinePath -Raw | ConvertFrom-Json
$baselineByKey = @{}
foreach ($record in @($baseline.records)) {
    $baselineByKey[[string]$record.key] = $record
}
$currentByKey = @{}
foreach ($record in @($current.records)) {
    $currentByKey[[string]$record.key] = $record
}

$problems = [Collections.Generic.List[string]]::new()
foreach ($key in @($baselineByKey.Keys + $currentByKey.Keys | Sort-Object -Unique)) {
    if (-not $baselineByKey.ContainsKey($key)) {
        $problems.Add("added: $key")
        continue
    }
    if (-not $currentByKey.ContainsKey($key)) {
        $problems.Add("removed: $key")
        continue
    }
    $expected = $baselineByKey[$key]
    $actual = $currentByKey[$key]
    if ($expected.hash -ne $actual.hash) {
        $problems.Add(
            "changed: $key status $($expected.status) -> $($actual.status), steps $($expected.step_count) -> $($actual.step_count)"
        )
    }
}

if ($problems.Count -gt 0) {
    $reported = @($problems | Select-Object -First 30)
    $remaining = $problems.Count - $reported.Count
    $suffix = if ($remaining -gt 0) { "`n... and $remaining more" } else { "" }
    throw "Mission route corpus differs from the reviewed baseline:`n$($reported -join "`n")$suffix"
}

Write-Host "PASS: $($current.record_count) mission routes match the reviewed corpus baseline"
