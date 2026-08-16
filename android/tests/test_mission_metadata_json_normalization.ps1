#!/usr/bin/env pwsh

$ErrorActionPreference = "Stop"
$androidRoot = Split-Path -Parent $PSScriptRoot
$normalizer = Join-Path $androidRoot "helpers\normalize_json.py"
$python = Get-Command python -ErrorAction SilentlyContinue
$usePyLauncher = $false
if (-not $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
    $usePyLauncher = $true
}
if (-not $python) {
    throw "Python not found for mission metadata JSON normalization test"
}

function Invoke-MetadataNormalizer {
    param([Parameter(Mandatory = $true)][string]$Text)

    $arguments = @()
    if ($usePyLauncher) { $arguments += "-3" }
    $arguments += @($normalizer, "--mission-metadata")
    $result = $Text | & $python.Source @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Mission metadata normalizer failed with exit code $LASTEXITCODE"
    }
    return (($result -join "`n") + "`n")
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
