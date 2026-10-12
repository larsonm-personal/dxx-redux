#!/usr/bin/env pwsh
# Compare UUD runtime CPU pixels/flags against independently decoded author sources
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
$inputs = Join-Path $repoRoot 'android/temp/uud-indexed-fixture-inputs'
New-Item -ItemType Directory -Path $inputs -Force | Out-Null
$expected = Join-Path $inputs 'uud-indexed-expectations.json'
& python (Join-Path $repoRoot 'game_data/mods/xfing/audit_texture_conversion.py') --runtime-expectations $expected
if ($LASTEXITCODE -ne 0) { throw 'Independent source conversion audit failed' }
$descent = Join-Path $inputs 'D2DESCENT.PIG'
Copy-Item -LiteralPath (Join-Path $sourceRoot 'baseline-d2-descent/original-2015/DESCENT.PIG') -Destination $descent
$ham = Join-Path $inputs 'UUD2.HAM'
$archive = [IO.Compression.ZipFile]::OpenRead((Join-Path $sourceRoot 'uud2tp/uud2tp.dxa'))
try {
    $entry = @($archive.Entries | Where-Object FullName -IEQ 'descent2.ham')[0]
    if (-not $entry) { throw 'Author DXA is missing DESCENT2.HAM' }
    [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $ham, $true)
} finally { $archive.Dispose() }
$dataFiles = @($expected, $descent, $ham)
foreach ($name in @('DESCENT.PIG', 'DESCENT2.HAM', 'DESCENT2.S22', 'GROUPA.PIG', 'ALIEN1.PIG', 'ALIEN2.PIG', 'FIRE.PIG', 'ICE.PIG', 'WATER.PIG')) {
    $dataFiles += Join-Path $baseline $name
}
foreach ($game in @('d1', 'd2')) { $dataFiles += Join-Path $sourceRoot "tmp/plain_texture_dxa/uu${game}tp-textures.dxa" }
& (Join-Path $PSScriptRoot '../helpers/run_native_engine_fixture.ps1') -Fixture mod_bitmaps -ExpectedCases 8 -DataFiles $dataFiles @PSBoundParameters
