#!/usr/bin/env pwsh
param(
    [string]$OutputPath = "",
    [string]$OutputDir = "",
    [string]$BaselineRoot = "",
    [string]$SourceDir = "",
    [string]$OriginalDxaPath = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..\..")
. (Join-Path $scriptDir "uud2sp_ham_patch_lib.ps1")

if (-not $OutputPath) {
    $OutputPath = Join-Path $scriptDir "UUD2SP1_4.no_ham.dxa"
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $scriptDir "tmp\uud2sp_ham_patch_dxa"
}
if (-not $BaselineRoot) {
    $BaselineRoot = Join-Path $repoRoot "game_data_to_copy_to_emulator\temp"
}
if (-not $SourceDir) {
    $SourceDir = Join-Path $scriptDir "uud2sp"
}
if (-not $OriginalDxaPath) {
    $OriginalDxaPath = Join-Path $scriptDir "UUD2SP1_4.dxa"
}

function Get-Uud2spDisplayPath {
    param([string]$Path)

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    return [System.IO.Path]::GetRelativePath($repoRoot.Path, $resolved).Replace('\', '/')
}

function Join-Uud2spStagePath {
    param([string]$StageRoot, [string]$ZipPath)

    return Join-Path $StageRoot ($ZipPath -replace '/', [System.IO.Path]::DirectorySeparatorChar)
}

function New-Uud2spRequiredBaseFile {
    param(
        [string]$Path,
        [string]$Sha256,
        [long]$Size
    )

    return [pscustomobject]@{
        game = "d2"
        role = "baselineHam"
        filename = "descent2.ham"
        path = Get-Uud2spDisplayPath $Path
        sha256 = $Sha256
        size = $Size
        version = "D2 v1.2 DESCENT2.HAM"
        required = $true
        reason = "UUD2SP HAM patch test operations were generated against this metadata baseline"
        patchPaths = @($script:Uud2spHamPatchPath)
    }
}

function Get-Uud2spReadmeText {
    return @"
# UUD2SP

Translated to DXA by the Android Redux project
See the conversion scripts on GitHub
"@
}

function Get-Uud2spSha256ForFile {
    param([string]$Path)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace("-", "").ToLowerInvariant()
    } finally {
        $stream.Dispose()
        $sha.Dispose()
    }
}

function Copy-Uud2spFileToStage {
    param(
        [string]$SourcePath,
        [string]$StageRoot,
        [string]$ZipPath
    )

    $target = Join-Uud2spStagePath -StageRoot $StageRoot -ZipPath $ZipPath
    $parent = Split-Path -Parent $target
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    Copy-Item -LiteralPath $SourcePath -Destination $target -Force
}

$baseHamPath = Join-Path $BaselineRoot "DESCENT2.HAM"
$patchedHamPath = Join-Path $SourceDir "descent2.ham"
$rtfPath = Join-Path $SourceDir "UUD2SP.rtf"
$soundPaths = @(
    [pscustomobject]@{ Source = Join-Path $SourceDir "descent2.s11"; ZipPath = "descent2.s11"; SampleRate = 11025 },
    [pscustomobject]@{ Source = Join-Path $SourceDir "descent2.s22"; ZipPath = "descent2.s22"; SampleRate = 22050 }
)

foreach ($path in @($baseHamPath, $patchedHamPath, $rtfPath, $OriginalDxaPath) + @($soundPaths | ForEach-Object Source)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required input: $path"
    }
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$stageRoot = Join-Path $OutputDir "stage"
if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $stageRoot | Out-Null

Write-Host "Analyzing UUD2SP HAM changes"
$analysis = New-Uud2spHamPatchAnalysis -BaseHamPath $baseHamPath -PatchedHamPath $patchedHamPath
$sectionDeltaCounts = [ordered]@{}
foreach ($group in @($analysis.rows | Group-Object section | Sort-Object Name)) {
    $sectionDeltaCounts[$group.Name] = $group.Count
}

$summary = [pscustomobject]@{
    basePath = Get-Uud2spDisplayPath $baseHamPath
    patchPath = Get-Uud2spDisplayPath $patchedHamPath
    baseSha256 = $analysis.baseSha256
    patchSha256 = $analysis.patchSha256
    baseSize = $analysis.baseSize
    patchSize = $analysis.patchSize
    hamMainLength = $analysis.hamMainLength
    bitmapXlatOffset = $analysis.bitmapXlatOffset
    bitmapXlatCount = $analysis.bitmapXlatCount
    engineReadableMainLength = $analysis.engineReadableMainLength
    originalEngineComparableSha256 = $analysis.originalEngineComparableSha256
    engineComparableLength = $analysis.engineComparableLength
    engineComparableSha256 = $analysis.engineComparableSha256
    generatedComparableSha256 = $analysis.generatedComparableSha256
    ignoredOriginalTrailer = [pscustomobject]@{
        offset = $analysis.ignoredTrailerOffset
        size = $analysis.ignoredTrailerLength
        sha256 = $analysis.ignoredTrailerSha256
        reason = "HAXMED trailer after the retail-length HAM prefix, intentionally not included in the DXA"
    }
    counts = $analysis.counts
    sourceFieldDeltaCount = $analysis.sourceChangedFieldCount
    fieldDeltaCount = $analysis.changedFieldCount
    combinedAlignmentFieldDeltaCount = $analysis.combinedAlignmentFieldCount
    operationCount = $analysis.operationCount
    sectionDeltaCounts = [pscustomobject]$sectionDeltaCounts
    fieldDeltas = @($analysis.rows)
    combinedAlignmentFieldDeltas = @($analysis.combinedAlignmentRows)
}

Write-XfingJsonFile -Path (Join-Uud2spStagePath -StageRoot $stageRoot -ZipPath $script:Uud2spHamPatchPath) -Value @($analysis.patchOperations)
Write-XfingJsonFile -Path (Join-Uud2spStagePath -StageRoot $stageRoot -ZipPath $script:Uud2spHamPatchSummaryPath) -Value $summary

foreach ($soundPath in $soundPaths) {
    Copy-Uud2spFileToStage -SourcePath $soundPath.Source -StageRoot $stageRoot -ZipPath $soundPath.ZipPath
}
Copy-Uud2spFileToStage -SourcePath $rtfPath -StageRoot $stageRoot -ZipPath "UUD2SP.rtf"
Set-Content -LiteralPath (Join-Path $stageRoot "README.md") -Value (Get-Uud2spReadmeText) -Encoding utf8

$requiredBaseFiles = @(
    New-Uud2spRequiredBaseFile -Path $baseHamPath -Sha256 $analysis.baseSha256 -Size $analysis.baseSize
)
$soundFiles = @($soundPaths | ForEach-Object {
        [pscustomobject]@{
            path = $_.ZipPath
            sourcePath = Get-Uud2spDisplayPath $_.Source
            sha256 = Get-Uud2spSha256ForFile $_.Source
            size = (Get-Item -LiteralPath $_.Source).Length
            sampleRate = $_.SampleRate
        }
    })
$sourceFiles = @($requiredBaseFiles)
$sourceFiles += [pscustomobject]@{
    role = "originalDxa"
    path = Get-Uud2spDisplayPath $OriginalDxaPath
    sha256 = Get-Uud2spSha256ForFile $OriginalDxaPath
    size = (Get-Item -LiteralPath $OriginalDxaPath).Length
}
$sourceFiles += [pscustomobject]@{
    role = "patchedHam"
    path = Get-Uud2spDisplayPath $patchedHamPath
    sha256 = $analysis.patchSha256
    size = $analysis.patchSize
    engineComparableSha256 = $analysis.engineComparableSha256
    originalEngineComparableSha256 = $analysis.originalEngineComparableSha256
    ignoredTrailerSha256 = $analysis.ignoredTrailerSha256
    ignoredTrailerSize = $analysis.ignoredTrailerLength
    combinedAlignmentFieldDeltas = @($analysis.combinedAlignmentRows)
}
$sourceFiles += @($soundFiles | ForEach-Object {
        [pscustomobject]@{ role = "soundFile"; path = $_.sourcePath; sha256 = $_.sha256; size = $_.size; archivePath = $_.path }
    })
$sourceFiles += [pscustomobject]@{ role = "documentation"; path = Get-Uud2spDisplayPath $rtfPath; sha256 = Get-Uud2spSha256ForFile $rtfPath; size = (Get-Item -LiteralPath $rtfPath).Length; archivePath = "UUD2SP.rtf" }

$requiredBaseDescription = "D2 retail v1.2 data with the standard DESCENT2.HAM hash, such as GOG or Steam installs that preserve that file"
$manifest = [pscustomobject]@{
    schema = "com.dxxredux.sound-patch-dxa.v1"
    pack = "uud2sp"
    game = "d2"
    generatedUtc = (Get-Date).ToUniversalTime().ToString("o")
    notes = @(
        "Required base files: $requiredBaseDescription",
        "Sound payloads are the UUD2SP Descent2.S11 and Descent2.S22 files",
        "No full HAM file is included",
        "HAM changes use RFC 6902 JSON Patch against semantic engine fields",
        "Overlapping HAM field values are aligned to Xfing's D2TP+SP combined pack",
        "The original HAXMED HAM trailer is recorded in metadata and intentionally omitted",
        "Patch verification compares the retail-length HAM prefix, including GameBitmapXlat"
    )
    compatibility = [pscustomobject]@{
        requiredBaseDescription = $requiredBaseDescription
        requiredBaseFiles = @($requiredBaseFiles)
        preflight = "Compare requiredBaseFiles against the active game data SHA-256 before mounting this DXA"
        patchModel = "RFC 6902 JSON Patch operations target semantic HAM fields and include test operations"
    }
    sourceFiles = @($sourceFiles)
    d2 = [pscustomobject]@{
        soundFiles = @($soundFiles)
        hamPatchFormat = "RFC 6902 JSON Patch"
        hamPatchTarget = "com.dxxredux.d2-ham-fields.v1"
        hamPatchPath = $script:Uud2spHamPatchPath
        hamPatchSummaryPath = $script:Uud2spHamPatchSummaryPath
        hamPatchOperationCount = $analysis.operationCount
        hamFieldDeltaCount = $analysis.changedFieldCount
        combinedAlignmentHamFieldDeltaCount = $analysis.combinedAlignmentFieldCount
        hamMainLength = $analysis.hamMainLength
        bitmapXlatCount = $analysis.bitmapXlatCount
        engineComparableHamSha256 = $analysis.engineComparableSha256
        originalEngineComparableHamSha256 = $analysis.originalEngineComparableSha256
        ignoredOriginalHamTrailer = $summary.ignoredOriginalTrailer
    }
}

Write-XfingJsonFile -Path (Join-Path $stageRoot "metadata\manifest.json") -Value $manifest
New-XfingDxaFromDirectory -SourceDir $stageRoot -OutputPath $OutputPath
Write-Host "Created $OutputPath"

[pscustomobject]@{
    path = $OutputPath
    archiveBytes = (Get-Item -LiteralPath $OutputPath).Length
    hamFieldDeltas = $analysis.changedFieldCount
    combinedAlignmentHamFieldDeltas = $analysis.combinedAlignmentFieldCount
    hamPatchOperations = $analysis.operationCount
    soundFiles = @($soundFiles).Count
    ignoredOriginalHamTrailerBytes = $analysis.ignoredTrailerLength
}