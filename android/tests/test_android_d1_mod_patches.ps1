#!/usr/bin/env pwsh
# Compare converted UUD patches with author-supplied levels through both real engines
[CmdletBinding()]
param(
    [string]$Serial = $env:ANDROID_SERIAL,
    [string]$NativeBuildDir,
    [string]$OutputDirectory,
    [switch]$NoEngineBuild
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
$sourceRoot = Join-Path $repoRoot 'game_data/mods/xfing/dxx_tp'
$baseline = Join-Path $repoRoot 'game_data_to_copy_to_emulator/temp'
$inputs = Join-Path $repoRoot 'android/temp/uud-d1-fixture-inputs'
New-Item -ItemType Directory -Path $inputs -Force | Out-Null
$expected = Join-Path $inputs 'uud-indexed-expectations.json'
& python (Join-Path $repoRoot 'game_data/mods/xfing/audit_texture_conversion.py') --runtime-expectations $expected
if ($LASTEXITCODE -ne 0) { throw 'Independent source conversion audit failed' }
$dataFiles = @(
    $expected,
    (Join-Path $baseline 'DESCENT.PIG'),
    (Join-Path $baseline 'DESCENT.HOG'),
    (Join-Path $sourceRoot 'tmp/plain_texture_dxa/uud1tp-textures.dxa')
)
foreach ($level in @(3, 17, 20)) {
    $name = 'level{0:D2}.rdl' -f $level
    $destination = Join-Path $inputs "expected-$name"
    Copy-Item -LiteralPath (Join-Path $sourceRoot "uud1tp/$name") -Destination $destination
    $dataFiles += $destination
}
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture d1_mod_patches -ExpectedCases 17 -DataFiles $dataFiles @PSBoundParameters
