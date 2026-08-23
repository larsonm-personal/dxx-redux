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

Write-Host "PASS: Android and host mission metadata numeric forms converge to byte-stable JSON"
