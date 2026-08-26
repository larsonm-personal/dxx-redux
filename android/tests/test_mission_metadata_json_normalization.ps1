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
foreach ($expected in @('"mine_volume": 24904610.0', '"travel_distance": 0.0', '"distance": 580.0', '"x": 514.0')) {
    if (-not $androidNormalized.Contains($expected)) {
        throw "Canonical mission metadata output is missing $expected"
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
