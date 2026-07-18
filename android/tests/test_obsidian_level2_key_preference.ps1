#!/usr/bin/env pwsh
param(
    [string]$MetadataPath = ''
)

$ErrorActionPreference = 'Stop'

if (-not $MetadataPath) {
    $MetadataPath = Join-Path (Split-Path (Split-Path $PSScriptRoot)) 'game_data\mission_files\Obsidian.json'
}

if (-not (Test-Path -LiteralPath $MetadataPath -PathType Leaf)) {
    throw "Obsidian metadata file not found: $MetadataPath"
}

$metadata = Get-Content -LiteralPath $MetadataPath -Raw | ConvertFrom-Json
$level = @($metadata.levels | Where-Object { $_.level_num -eq 2 })
if ($level.Count -ne 1) {
    throw "Expected exactly one Obsidian level 2 record, found $($level.Count)"
}
if ($level[0].route_status -ne 'ok') {
    throw "Obsidian level 2 route is $($level[0].route_status): $($level[0].route_problem)"
}

$steps = @($level[0].route_steps)
$objectives = @($steps | Where-Object { $_.kind -ne 'start' })
$expected = @(
    @{ Kind = 'key'; Key = 'blue'; Trigger = $null },
    @{ Kind = 'trigger'; Key = $null; Trigger = 4 },
    @{ Kind = 'trigger'; Key = $null; Trigger = 5 },
    @{ Kind = 'trigger'; Key = $null; Trigger = 10 },
    @{ Kind = 'reactor'; Key = $null; Trigger = $null },
    @{ Kind = 'trigger'; Key = $null; Trigger = 11 },
    @{ Kind = 'exit'; Key = $null; Trigger = 3 }
)
if ($objectives.Count -ne $expected.Count) {
    throw "Expected $($expected.Count) objectives, found $($objectives.Count)"
}
for ($index = 0; $index -lt $expected.Count; $index++) {
    $actual = $objectives[$index]
    $wanted = $expected[$index]
    if ($actual.kind -ne $wanted.Kind -or
        ($null -ne $wanted.Key -and $actual.key -ne $wanted.Key) -or
        ($null -ne $wanted.Trigger -and $actual.trigger -ne $wanted.Trigger)) {
        throw "Objective $($index + 1) is $($actual.kind) key=$($actual.key) trigger=$($actual.trigger)"
    }
}
if ($objectives[0].can_be_bypassed -ne $true) {
    throw 'Blue key is not marked can_be_bypassed'
}
if ($level[0].route_note -notlike '*blue key can be skipped*transparent wall*') {
    throw "Unexpected route note: $($level[0].route_note)"
}
if (@($level[0].notes | Where-Object { $_ -eq 'blue key not necessary' }).Count -ne 0) {
    throw 'Obsidian level 2 incorrectly marks its preferred blue key unnecessary'
}

Write-Host 'PASS Obsidian level 2 prefers and annotates the blue-key route'
