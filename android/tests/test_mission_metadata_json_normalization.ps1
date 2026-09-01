#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $androidRoot "helpers\normalized_json_text.ps1")

function Invoke-MetadataNormalizer {
    param([Parameter(Mandatory = $true)][string]$Text)

    return ConvertTo-NormalizedJsonText -Text $Text -MissionMetadata
}

$quotedArgument = ConvertTo-WindowsProcessArgument 'C:\path with spaces\trailing\'
if ($quotedArgument -cne '"C:\path with spaces\trailing\\"') {
    throw "Windows process argument quoting is incorrect: $quotedArgument"
}

$androidStyle = '{"levels":[{"mine_volume":24904610,"travel_distance":0,"route_steps":[{"distance":580,"label_pos":{"x":514,"y":-22,"z":0}}],"replacements":[{"kind":"robot"}],"replacement_groups":[{"kind":"robot_changes"}]}]}'
$hostStyle = '{"levels":[{"mine_volume":24904610.0,"travel_distance":0.0,"route_steps":[{"distance":580.0,"label_pos":{"x":514.0,"y":-22.0,"z":0.0}}],"replacements":[{"kind":"robot"}],"replacement_groups":[{"kind":"robot_changes"}]}]}'

$androidNormalized = Invoke-MetadataNormalizer -Text $androidStyle
$hostNormalized = Invoke-MetadataNormalizer -Text $hostStyle
if ($androidNormalized -cne $hostNormalized) {
    throw "Android and host mission metadata did not normalize to identical output"
}

$powerShell7Style = '{"mine_volume":50869833.801380076,"mine_volume_normalized":10.468290463576233,"travel_distance":14106.360626121883}'
$powerShell51Style = '{"mine_volume":50869833.80138008,"mine_volume_normalized":10.468290463576231,"travel_distance":14106.360626121885}'
if ((Invoke-MetadataNormalizer -Text $powerShell7Style) -cne (Invoke-MetadataNormalizer -Text $powerShell51Style)) {
    throw "PowerShell 5.1 and 7 mission metadata float forms did not normalize identically"
}
foreach ($expected in @('"mine_volume": 24904600.0', '"travel_distance": 0.0', '"distance": 580.0', '"x": 514.0')) {
    if (-not $androidNormalized.Contains($expected)) {
        throw "Canonical mission metadata output is missing $expected"
    }
}

$precisionInput = '{"mine_volume":50869833.801380076,"mine_volume_normalized":10.468290463576233,' +
'"travel_distance":14106.360626121883,"label_pos":{"x":-250.8622589111328,"y":-0.0001,"z":500.0885772705078}}'
$precisionNormalized = Invoke-MetadataNormalizer -Text $precisionInput
foreach ($expected in @(
        '"mine_volume": 50869800.0',
        '"mine_volume_normalized": 10.4683',
        '"travel_distance": 14106.4',
        '"x": -250.9',
        '"y": 0.0',
        '"z": 500.1'
    )) {
    if (-not $precisionNormalized.Contains($expected)) {
        throw "Gameplay-scale mission metadata output is missing $expected"
    }
}
foreach ($excluded in @('"replacements"', '"replacement_groups"')) {
    if ($androidNormalized.Contains($excluded)) {
        throw "Canonical mission metadata output should exclude $excluded"
    }
}

$secondPass = Invoke-MetadataNormalizer -Text $androidNormalized
if ($secondPass -cne $androidNormalized) {
    throw "Mission metadata normalization is not idempotent"
}

$structuredIntent = '{"mission_filename":"example.mn2","mission_intent":{' +
'"classification":"single_player_or_coop","rule":"campaign_actors","confidence":"high",' +
'"reason":"Campaign actors","declarations":{"anarchy_only":false,"normal":false,"coop":false,' +
'"anarchy":false,"robo_anarchy":false,"capture_flag":false,"hoard":false},' +
'"normal_levels":1,"campaign_actor_levels":1,' +
'"arena_like_levels":0,"solo_like_levels":0,"player_start_min":1,"player_start_max":1,' +
'"coop_start_min":3,"coop_start_max":3,"robots":10,"hostages":0,"matcens":1,' +
'"guidebots":1,"powerups":20,"reactors":1}}'
[void](Invoke-MetadataNormalizer -Text $structuredIntent)
try {
    [void](Invoke-MetadataNormalizer -Text '{"mission_filename":"example.mn2","mission_intent":"single_player_or_coop"}')
    throw "Scalar mission_intent unexpectedly passed mission metadata validation"
} catch {
    if ($_.Exception.Message -notmatch 'JSON formatter failed') { throw }
}

$variantInput = '[{"mission_filename":"ULTERIOR.MN2","mission_path":"REBIRTH/ULTERIOR.MN2","target_index":9},' +
'{"mission_filename":"ULTERIOR.MN2","mission_path":"D2X/ULTERIOR.MN2","target_index":8},' +
'{"mission_filename":"ULTERIOR.MN2","mission_path":"DOS/ULTERIOR.MN2","target_index":7}]'
$variants = Invoke-MetadataNormalizer -Text $variantInput | ConvertFrom-Json
if ((@($variants.mission_path) -join ',') -cne `
        'D2X/ULTERIOR.MN2,DOS/ULTERIOR.MN2,REBIRTH/ULTERIOR.MN2' -or
    (@($variants.target_index) -join ',') -cne '0,1,2') {
    throw "Mission variants are not canonically ordered by archive-relative path"
}

Write-Host "PASS: Android and host mission metadata numeric forms converge to byte-stable JSON"
