#!/usr/bin/env pwsh
param(
    [ValidateSet("d1", "d2", "both")]
    [string]$Game = "both",

    [string]$OutputDir = "",

    [string]$BaselineRoot = "",

    [string]$Uud1Root = "",

    [string]$Uud2Root = "",

    [string]$D2DescentBaselinePig = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")
. (Join-Path $scriptDir "xfing_minimal_dxa_lib.ps1")

if (-not $OutputDir) {
    $OutputDir = Join-Path $scriptDir "dxx_tp\tmp\plain_texture_dxa"
}
if (-not $BaselineRoot) {
    $BaselineRoot = Join-Path $repoRoot "game_data_to_copy_to_emulator\temp"
}
if (-not $Uud1Root) {
    $Uud1Root = Join-Path $scriptDir "dxx_tp\uud1tp"
}
if (-not $Uud2Root) {
    $Uud2Root = Join-Path $scriptDir "dxx_tp\uud2tp"
}

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

function Get-XfingDisplayPath {
    param([string]$Path)

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    return [System.IO.Path]::GetRelativePath($repoRoot.Path, $resolved).Replace('\', '/')
}

function New-XfingStageRoot {
    param([string]$Name)

    $stageRoot = Join-Path $OutputDir "stage\$Name"
    if (Test-Path -LiteralPath $stageRoot) {
        Remove-Item -LiteralPath $stageRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Path $stageRoot | Out-Null
    return $stageRoot
}

function Join-XfingStagePath {
    param(
        [string]$StageRoot,
        [string]$ZipPath
    )

    return Join-Path $StageRoot ($ZipPath -replace '/', [System.IO.Path]::DirectorySeparatorChar)
}

function Get-XfingTextureZipPath {
    param(
        [string]$ZipPrefix,
        [string]$SafeName,
        [int]$Index,
        [bool]$NeedsIndexPath,
        [bool]$IsMask
    )

    $stem = if ($NeedsIndexPath) { "{0:D5}_{1}" -f $Index, $SafeName } else { $SafeName }
    if ($IsMask) {
        $stem = "${stem}_mask"
    }
    $fileName = "$stem.png"
    if ($ZipPrefix) {
        if ($NeedsIndexPath) {
            return "$ZipPrefix/by-index/$fileName"
        }
        return "$ZipPrefix/$fileName"
    }
    if ($NeedsIndexPath) {
        return "textures/d1/by-index/$fileName"
    }
    return $fileName
}

function Get-XfingPackReadmeText {
    param([string]$PackName)

    return @"
# $PackName

Translated to DXA by the Android Redux project
See the conversion scripts on GitHub
"@
}

function Add-XfingDocumentation {
    param(
        [string]$StageRoot,
        [string]$SourceDocPath,
        [string]$PackName
    )

    if (-not (Test-Path -LiteralPath $SourceDocPath)) {
        throw "Missing documentation source: $SourceDocPath"
    }

    Copy-Item -LiteralPath $SourceDocPath -Destination (Join-Path $StageRoot ([System.IO.Path]::GetFileName($SourceDocPath))) -Force
    Set-Content -LiteralPath (Join-Path $StageRoot "README.md") -Value (Get-XfingPackReadmeText -PackName $PackName) -Encoding utf8
}

function New-XfingRequiredBaseFile {
    param(
        [string]$GameName,
        [string]$Role,
        [string]$Path,
        [string]$Sha256,
        [long]$Size,
        [string]$Version,
        [string]$Reason,
        [string[]]$PatchPaths = @(),
        [bool]$Required = $true
    )

    return [pscustomobject]@{
        game = $GameName
        role = $Role
        filename = [System.IO.Path]::GetFileName($Path).ToLowerInvariant()
        path = Get-XfingDisplayPath $Path
        sha256 = $Sha256
        size = $Size
        version = $Version
        required = $Required
        reason = $Reason
        patchPaths = @($PatchPaths)
    }
}

function New-XfingCompatibilityBlock {
    param(
        [string]$Description,
        [object[]]$RequiredBaseFiles
    )

    return [pscustomobject]@{
        requiredBaseDescription = $Description
        requiredBaseFiles = @($RequiredBaseFiles)
        preflight = "Compare each requiredBaseFiles entry against the active game data SHA-256 before mounting this DXA"
        patchModel = "RFC 6902 JSON Patch operations target semantic engine documents and include test operations for changed rows"
    }
}

function Add-XfingTextureImages {
    param(
        [string]$GameName,
        [string]$PigKey,
        $BasePig,
        $PatchPig,
        [string]$StageRoot,
        [string]$ZipPrefix,
        [byte[]]$Palette
    )

    $rows = Compare-XfingPigEntriesByIndex -BasePig $BasePig -PatchPig $PatchPig
    $changedRows = @($rows | Where-Object { $_.Status -eq "Changed" -or $_.Status -eq "Extra" })
    $nameCounts = @{}
    foreach ($row in $changedRows) {
        if (-not $row.Patch) {
            continue
        }
        $safeName = (Get-XfingSafeName $row.Patch.Name).ToLowerInvariant()
        if (-not $nameCounts.ContainsKey($safeName)) {
            $nameCounts[$safeName] = 0
        }
        $nameCounts[$safeName]++
    }

    $textures = @()
    foreach ($row in $changedRows) {
        $patchEntry = $row.Patch
        if (-not $patchEntry) {
            continue
        }
        $baseEntry = $row.Base
        $displayName = Get-XfingSafeName $patchEntry.Name
        $safeName = if ($row.Status -eq "Extra") { "idx$($patchEntry.Index)" } else { $displayName }
        $needsIndexPath = ($row.Status -ne "Extra") -and ($nameCounts[$safeName.ToLowerInvariant()] -gt 1)
        $zipPath = Get-XfingTextureZipPath `
            -ZipPrefix $ZipPrefix `
            -SafeName $safeName `
            -Index $patchEntry.Index `
            -NeedsIndexPath $needsIndexPath `
            -IsMask $false
        $maskZipPath = Get-XfingTextureZipPath `
            -ZipPrefix $ZipPrefix `
            -SafeName $safeName `
            -Index $patchEntry.Index `
            -NeedsIndexPath $needsIndexPath `
            -IsMask $true
        $outputPath = Join-XfingStagePath -StageRoot $StageRoot -ZipPath $zipPath
        $maskOutputPath = Join-XfingStagePath -StageRoot $StageRoot -ZipPath $maskZipPath
        $png = Export-XfingBitmapEntryPng `
            -Pig $PatchPig `
            -Entry $patchEntry `
            -Palette $Palette `
            -OutputPath $outputPath `
            -MaskOutputPath $maskOutputPath
        $textures += [pscustomobject]@{
            status = $row.Status
            path = $zipPath
            maskPath = if ($png.maskSha256) { $maskZipPath } else { $null }
            name = $displayName
            rawName = $patchEntry.RawName
            baseIndex = if ($baseEntry) { $baseEntry.Index } else { $null }
            patchIndex = $patchEntry.Index
            width = $patchEntry.Width
            height = $patchEntry.Height
            flags = $patchEntry.Flags
            dflags = $patchEntry.DFlags
            avgColor = $patchEntry.AvgColor
            sourcePayloadSha256 = $patchEntry.Hash
            basePayloadSha256 = if ($baseEntry) { $baseEntry.Hash } else { $null }
            pngSha256 = $png.sha256
            maskSha256 = $png.maskSha256
            byIndexPath = $needsIndexPath
        }
    }

    return [pscustomobject]@{
        game = $GameName
        pig = $PigKey
        basePath = Get-XfingDisplayPath $BasePig.Path
        patchPath = Get-XfingDisplayPath $PatchPig.Path
        baseSha256 = $BasePig.Sha256
        patchSha256 = $PatchPig.Sha256
        baseBitmapCount = $BasePig.BitmapCount
        patchBitmapCount = $PatchPig.BitmapCount
        same = @($rows | Where-Object Status -eq "Same").Count
        changed = @($rows | Where-Object Status -eq "Changed").Count
        extra = @($rows | Where-Object Status -eq "Extra").Count
        missing = @($rows | Where-Object Status -eq "Missing").Count
        textureCount = @($textures).Count
        byIndexTextureCount = @($textures | Where-Object byIndexPath).Count
        texturePngBytes = (@($textures) | ForEach-Object { (Get-Item -LiteralPath (Join-XfingStagePath -StageRoot $StageRoot -ZipPath $_.path)).Length } | Measure-Object -Sum).Sum
        textures = @($textures)
    }
}

function Get-XfingPaletteFromHog {
    param(
        [string]$HogPath,
        [string]$EntryName,
        [string]$WorkDir
    )

    $palettePath = Join-Path $WorkDir $EntryName
    Export-XfingHogEntry -HogPath $HogPath -EntryName $EntryName -OutputPath $palettePath
    return Read-XfingPaletteBytes $palettePath
}

function New-XfingD1Archive {
    $stageRoot = New-XfingStageRoot "d1"
    $metadataRoot = Join-Path $stageRoot "metadata"
    $basePigPath = Join-Path $BaselineRoot "DESCENT.PIG"
    $baseHogPath = Join-Path $BaselineRoot "DESCENT.HOG"
    $patchPigPath = Join-Path $Uud1Root "descent.pig"
    Add-XfingDocumentation -StageRoot $stageRoot -SourceDocPath (Join-Path $Uud1Root "uud1tp.rtf") -PackName "uud1tp"
    foreach ($path in @($basePigPath, $baseHogPath, $patchPigPath)) {
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Missing required input: $path"
        }
    }

    Write-Host "Reading UUD1 PIG inputs"
    $palette = Get-XfingPaletteFromHog -HogPath $baseHogPath -EntryName "palette.256" -WorkDir (Join-Path $OutputDir "work\d1")
    $basePig = Read-XfingD1Pig $basePigPath
    $patchPig = Read-XfingD1Pig $patchPigPath
    $pigTextures = Add-XfingTextureImages `
        -GameName "d1" `
        -PigKey "descent.pig" `
        -BasePig $basePig `
        -PatchPig $patchPig `
        -StageRoot $stageRoot `
        -ZipPrefix "" `
        -Palette $palette

    $baseLevelDir = Join-Path $OutputDir "work\d1\base_levels"
    New-Item -ItemType Directory -Path $baseLevelDir -Force | Out-Null
    $levelNames = @("level03.rdl", "level17.rdl", "level20.rdl")
    $levelDeltas = @()
    $levelPatchOps = @()
    foreach ($levelName in $levelNames) {
        $baseLevelPath = Join-Path $baseLevelDir $levelName
        Export-XfingHogEntry -HogPath $baseHogPath -EntryName $levelName -OutputPath $baseLevelPath
        $patchLevelPath = Join-Path $Uud1Root $levelName
        if (-not (Test-Path -LiteralPath $patchLevelPath)) {
            throw "Missing patched level: $patchLevelPath"
        }
        $baseLevel = Read-XfingD1LevelSurfaces $baseLevelPath
        $patchLevel = Read-XfingD1LevelSurfaces $patchLevelPath
        $changes = Compare-XfingD1LevelSurfaces -BaseLevel $baseLevel -PatchLevel $patchLevel -LevelName $levelName
        $levelPatchOps += Convert-XfingD1SurfaceRowsToJsonPatch -Rows $changes
        $levelDeltas += [pscustomobject]@{
            level = $levelName
            baseSha256 = Get-XfingSha256ForFile $baseLevelPath
            patchSha256 = Get-XfingSha256ForFile $patchLevelPath
            baseSize = (Get-Item -LiteralPath $baseLevelPath).Length
            patchSize = (Get-Item -LiteralPath $patchLevelPath).Length
            changedSurfaces = @($changes).Count
            changes = @($changes)
        }
    }
    $levelPatchPath = "patches/d1/level_surface_patch.rfc6902.json"
    $levelSummaryPath = "patches/d1/level_surface_patch_summary.json"
    Write-XfingJsonFile -Path (Join-XfingStagePath -StageRoot $stageRoot -ZipPath $levelPatchPath) -Value @($levelPatchOps)
    Write-XfingJsonFile -Path (Join-XfingStagePath -StageRoot $stageRoot -ZipPath $levelSummaryPath) -Value @($levelDeltas)

    $requiredBaseDescription = "D1 retail v1.4a/v1.5 data with the standard DESCENT.PIG and DESCENT.HOG hashes, such as GOG or Infinite Abyss installs that preserve those files"
    $requiredBaseFiles = @(
        New-XfingRequiredBaseFile `
            -GameName "d1" `
            -Role "baselinePig" `
            -Path $basePigPath `
            -Sha256 $basePig.Sha256 `
            -Size (Get-Item -LiteralPath $basePigPath).Length `
            -Version "D1 v1.4a/v1.5 retail DESCENT.PIG" `
            -Reason "Texture replacement names and indices were generated against this PIG" `
            -PatchPaths @()
        New-XfingRequiredBaseFile `
            -GameName "d1" `
            -Role "baselineHog" `
            -Path $baseHogPath `
            -Sha256 (Get-XfingSha256ForFile $baseHogPath) `
            -Size (Get-Item -LiteralPath $baseHogPath).Length `
            -Version "D1 v1.4a/v1.5 retail DESCENT.HOG" `
            -Reason "Level-surface patch tests were generated against levels extracted from this HOG" `
            -PatchPaths @($levelPatchPath)
    )
    $sourceFiles = @($requiredBaseFiles)
    $sourceFiles += [pscustomobject]@{ role = "patchedPig"; path = Get-XfingDisplayPath $patchPigPath; sha256 = $patchPig.Sha256; size = (Get-Item -LiteralPath $patchPigPath).Length }
    $sourceFiles += @($levelDeltas | ForEach-Object {
            [pscustomobject]@{ role = "patchedLevel"; path = $_.level; sha256 = $_.patchSha256; size = $_.patchSize }
        })

    $manifest = [pscustomobject]@{
        schema = "com.dxxredux.plain-texture-dxa.v1"
        pack = "uud1tp"
        game = "d1"
        generatedUtc = (Get-Date).ToUniversalTime().ToString("o")
        notes = @(
            "Required base files: $requiredBaseDescription",
            "Texture payloads are standalone PNG files",
            "No full PIG, HOG, HAM, RDL, MN2, or palette files are included",
            "Root-level PNGs are named for the bitmap replacement lookup used by Redux",
            "Metadata patches use RFC 6902 JSON Patch against semantic engine documents"
        )
        compatibility = New-XfingCompatibilityBlock -Description $requiredBaseDescription -RequiredBaseFiles $requiredBaseFiles
        sourceFiles = @($sourceFiles)
        d1 = [pscustomobject]@{
            texturePigs = @($pigTextures)
            levelSurfacePatchFormat = "RFC 6902 JSON Patch"
            levelSurfacePatchTarget = "com.dxxredux.d1-level-surfaces.v1"
            levelSurfacePatchPath = $levelPatchPath
            levelSurfacePatchSummaryPath = $levelSummaryPath
            levelSurfacePatchOperationCount = @($levelPatchOps).Count
            levelSurfaceDeltaCount = (@($levelDeltas) | Measure-Object changedSurfaces -Sum).Sum
        }
    }

    Write-XfingJsonFile -Path (Join-Path $metadataRoot "manifest.json") -Value $manifest
    $outputPath = Join-Path $OutputDir "uud1tp-textures.dxa"
    New-XfingDxaFromDirectory -SourceDir $stageRoot -OutputPath $outputPath
    Write-Host "Created $outputPath"
    return [pscustomobject]@{
        path = $outputPath
        textures = $pigTextures.textureCount
        byIndexTextures = $pigTextures.byIndexTextureCount
        levelSurfacePatchOperations = $manifest.d1.levelSurfacePatchOperationCount
        levelSurfaceDeltas = $manifest.d1.levelSurfaceDeltaCount
        size = (Get-Item -LiteralPath $outputPath).Length
    }
}

$script:XfingD2SpCombinedWclipAlignment = @(
    [pscustomobject]@{ index = 5; values = [ordered]@{ PlayTime = 52428; OpenSound = 88; CloseSound = 89 } },
    [pscustomobject]@{ index = 9; values = [ordered]@{ PlayTime = 65536; OpenSound = 88; CloseSound = 89 } }
)

function Set-XfingD2SpCombinedWclipValues {
    param([object[]]$Rows)

    $aligned = @()
    foreach ($override in $script:XfingD2SpCombinedWclipAlignment) {
        $row = @($Rows | Where-Object { $_.section -eq "wclip" -and [int]$_.index -eq [int]$override.index }) | Select-Object -First 1
        if (-not $row) {
            throw "Missing UUD2TP wclip $($override.index) row for D2TP+SP combined alignment"
        }
        foreach ($property in $override.values.GetEnumerator()) {
            if (-not ($row.patch.PSObject.Properties.Name -contains $property.Key)) {
                throw "Missing UUD2TP wclip $($override.index) field $($property.Key) for D2TP+SP combined alignment"
            }
            $previousPatchValue = $row.patch.PSObject.Properties[$property.Key].Value
            $row.patch.PSObject.Properties[$property.Key].Value = $property.Value
            $aligned += [pscustomobject]@{
                section = "wclips"
                index = [int]$override.index
                field = $property.Key
                base = $row.base.PSObject.Properties[$property.Key].Value
                sourcePatch = $previousPatchValue
                patch = $property.Value
                reason = "Matches Xfing's D2TP+SP combined HAM"
            }
        }
    }
    return @($aligned)
}

function New-XfingHamDelta {
    param(
        [string]$StageRoot,
        [string]$PatchedHamPath
    )

    $baselineHam = Join-Path $BaselineRoot "DESCENT2.HAM"
    if (-not (Test-Path -LiteralPath $baselineHam)) {
        throw "Missing baseline HAM: $baselineHam"
    }
    $baseHam = Read-XfingHamSections $baselineHam
    $patchHam = Read-XfingHamSections $PatchedHamPath
    $textureRows = Compare-XfingRowsByJson -BaseRows $baseHam.Textures -PatchRows $patchHam.Textures -Section "texture"
    $vclipRows = Compare-XfingRowsByJson -BaseRows $baseHam.Vclips -PatchRows $patchHam.Vclips -Section "vclip"
    $eclipRows = Compare-XfingRowsByJson -BaseRows $baseHam.Eclips -PatchRows $patchHam.Eclips -Section "eclip"
    $wclipRows = Compare-XfingRowsByJson -BaseRows $baseHam.Walls -PatchRows $patchHam.Walls -Section "wclip"
    $spCombinedAlignment = Set-XfingD2SpCombinedWclipValues -Rows $wclipRows
    $patchOps = @()
    $patchOps += Convert-XfingRowsToJsonPatch -Rows $textureRows -BasePath "/sections/textures"
    $patchOps += Convert-XfingRowsToJsonPatch -Rows $vclipRows -BasePath "/sections/vclips"
    $patchOps += Convert-XfingRowsToJsonPatch -Rows $eclipRows -BasePath "/sections/eclips"
    $patchOps += Convert-XfingRowsToJsonPatch -Rows $wclipRows -BasePath "/sections/wclips"
    $delta = [pscustomobject]@{
        basePath = Get-XfingDisplayPath $baselineHam
        patchPath = Get-XfingDisplayPath $PatchedHamPath
        baseSha256 = $baseHam.Sha256
        patchSha256 = $patchHam.Sha256
        baseCounts = [pscustomobject]@{
            textures = $baseHam.TextureCount
            sounds = $baseHam.SoundCount
            vclips = $baseHam.VclipCount
            eclips = $baseHam.EclipCount
            wclips = $baseHam.WallCount
        }
        patchCounts = [pscustomobject]@{
            textures = $patchHam.TextureCount
            sounds = $patchHam.SoundCount
            vclips = $patchHam.VclipCount
            eclips = $patchHam.EclipCount
            wclips = $patchHam.WallCount
        }
        textureDeltas = @($textureRows)
        vclipDeltas = @($vclipRows)
        eclipDeltas = @($eclipRows)
        wclipDeltas = @($wclipRows)
        spCombinedAlignment = @($spCombinedAlignment)
    }
    $hamPatchPath = "patches/d2/ham_patch.rfc6902.json"
    $hamSummaryPath = "patches/d2/ham_patch_summary.json"
    Write-XfingJsonFile -Path (Join-XfingStagePath -StageRoot $StageRoot -ZipPath $hamPatchPath) -Value @($patchOps)
    Write-XfingJsonFile -Path (Join-XfingStagePath -StageRoot $StageRoot -ZipPath $hamSummaryPath) -Value $delta
    return [pscustomobject]@{
        path = $hamPatchPath
        summaryPath = $hamSummaryPath
        patchOperations = @($patchOps).Count
        delta = $delta
        textureDeltas = @($textureRows).Count
        vclipDeltas = @($vclipRows).Count
        eclipDeltas = @($eclipRows).Count
        wclipDeltas = @($wclipRows).Count
        spCombinedAlignmentDeltas = @($spCombinedAlignment).Count
    }
}

function New-XfingD2Archive {
    $stageRoot = New-XfingStageRoot "d2"
    $metadataRoot = Join-Path $stageRoot "metadata"
    $baseHogPath = Join-Path $BaselineRoot "DESCENT2.HOG"
    Add-XfingDocumentation -StageRoot $stageRoot -SourceDocPath (Join-Path $Uud2Root "UUD2TP.rtf") -PackName "uud2tp"
    if (-not (Test-Path -LiteralPath $baseHogPath)) {
        throw "Missing baseline HOG: $baseHogPath"
    }

    $pigPalettes = [ordered]@{
        "GROUPA.PIG" = "groupa.256"
        "ALIEN1.PIG" = "alien1.256"
        "ALIEN2.PIG" = "alien2.256"
        "FIRE.PIG" = "fire.256"
        "ICE.PIG" = "ice.256"
        "WATER.PIG" = "water.256"
    }
    $pigTextures = @()
    foreach ($pigName in $pigPalettes.Keys) {
        $basePath = Join-Path $BaselineRoot $pigName
        $patchPath = Join-Path $Uud2Root $pigName
        foreach ($path in @($basePath, $patchPath)) {
            if (-not (Test-Path -LiteralPath $path)) {
                throw "Missing required input: $path"
            }
        }
        $paletteName = $pigPalettes[$pigName]
        $paletteKey = [System.IO.Path]::GetFileNameWithoutExtension($paletteName).ToLowerInvariant()
        Write-Host "Reading UUD2 $pigName"
        $palette = Get-XfingPaletteFromHog -HogPath $baseHogPath -EntryName $paletteName -WorkDir (Join-Path $OutputDir "work\d2")
        $basePig = Read-XfingD2Pig $basePath
        $patchPig = Read-XfingD2Pig $patchPath
        $pigTextures += Add-XfingTextureImages `
            -GameName "d2" `
            -PigKey $pigName `
            -BasePig $basePig `
            -PatchPig $patchPig `
            -StageRoot $stageRoot `
            -ZipPrefix "textures/d2/sets/$paletteKey" `
            -Palette $palette
    }

    $patchedHamPath = Join-Path $OutputDir "work\d2\DESCENT2.HAM"
    if (-not (Test-Path -LiteralPath $patchedHamPath)) {
        Export-XfingZipEntry -ZipPath (Join-Path $Uud2Root "uud2tp.dxa") -EntryName "DESCENT2.HAM" -OutputPath $patchedHamPath
    }
    $hamDelta = New-XfingHamDelta -StageRoot $stageRoot -PatchedHamPath $patchedHamPath

    $skippedSources = @()
    $d2DescentPig = Join-Path $Uud2Root "DESCENT.PIG"
    if ($D2DescentBaselinePig -and (Test-Path -LiteralPath $D2DescentBaselinePig)) {
        $descentPalettePath = Join-Path $Uud2Root "descent.256"
        if (-not (Test-Path -LiteralPath $descentPalettePath)) {
            throw "D2 DESCENT.PIG conversion needs its matching descent.256 palette"
        }
        Write-Host "Reading UUD2 DESCENT.PIG with supplied baseline"
        $palette = Read-XfingPaletteBytes $descentPalettePath
        $basePig = Read-XfingD2Pig $D2DescentBaselinePig
        $patchPig = Read-XfingD2Pig $d2DescentPig
        $pigTextures += Add-XfingTextureImages `
            -GameName "d2" `
            -PigKey "DESCENT.PIG" `
            -BasePig $basePig `
            -PatchPig $patchPig `
            -StageRoot $stageRoot `
            -ZipPrefix "textures/d2/sets/descent" `
            -Palette $palette
    } elseif (Test-Path -LiteralPath $d2DescentPig) {
        $patchPig = Read-XfingD2Pig $d2DescentPig
        $skippedSources += [pscustomobject]@{
            path = Get-XfingDisplayPath $d2DescentPig
            reason = "No baseline D2 D1-palette PIG was supplied"
            bitmapCount = $patchPig.BitmapCount
            sha256 = $patchPig.Sha256
            size = (Get-Item -LiteralPath $d2DescentPig).Length
        }
    }
    foreach ($optionalName in @("descent.256", "pogtest.hog", "pogtest.mn2")) {
        $optionalPath = Join-Path $Uud2Root $optionalName
        if (Test-Path -LiteralPath $optionalPath) {
            $skippedSources += [pscustomobject]@{
                path = Get-XfingDisplayPath $optionalPath
                reason = "Optional/showcase source file is not needed for the plain texture DXA"
                sha256 = Get-XfingSha256ForFile $optionalPath
                size = (Get-Item -LiteralPath $optionalPath).Length
            }
        }
    }

    $textureSummaryPath = "metadata/d2/texture_summary.json"
    Write-XfingJsonFile -Path (Join-XfingStagePath -StageRoot $stageRoot -ZipPath $textureSummaryPath) -Value @($pigTextures)
    $requiredBaseDescription = "D2 retail v1.2 data with the standard DESCENT2.HAM and PIG hashes, such as GOG or Steam installs that preserve those files"
    $requiredBaseFiles = @(
        New-XfingRequiredBaseFile `
            -GameName "d2" `
            -Role "baselineHam" `
            -Path (Join-Path $BaselineRoot "DESCENT2.HAM") `
            -Sha256 $hamDelta.delta.baseSha256 `
            -Size (Get-Item -LiteralPath (Join-Path $BaselineRoot "DESCENT2.HAM")).Length `
            -Version "D2 v1.2 DESCENT2.HAM" `
            -Reason "HAM patch test operations were generated against this metadata baseline" `
            -PatchPaths @($hamDelta.path)
    )
    foreach ($texturePig in @($pigTextures)) {
        $basePigPath = Join-Path $BaselineRoot $texturePig.pig
        $requiredBaseFiles += New-XfingRequiredBaseFile `
            -GameName "d2" `
            -Role "baselinePig" `
            -Path $basePigPath `
            -Sha256 $texturePig.baseSha256 `
            -Size (Get-Item -LiteralPath $basePigPath).Length `
            -Version "D2 v1.2 $($texturePig.pig)" `
            -Reason "Texture replacement names and texture-set membership were generated against this PIG" `
            -PatchPaths @()
    }
    $manifest = [pscustomobject]@{
        schema = "com.dxxredux.plain-texture-dxa.v1"
        pack = "uud2tp"
        game = "d2"
        generatedUtc = (Get-Date).ToUniversalTime().ToString("o")
        notes = @(
            "Required base files: $requiredBaseDescription",
            "Texture payloads are standalone PNG files under texture-set directories",
            "No full PIG, HOG, HAM, RDL, MN2, or palette files are included",
            "D2 texture-set lookup or metadata import support is required before every PNG is directly playable",
            "Metadata patches use RFC 6902 JSON Patch against semantic engine documents"
        )
        compatibility = New-XfingCompatibilityBlock -Description $requiredBaseDescription -RequiredBaseFiles $requiredBaseFiles
        sourceFiles = @($requiredBaseFiles)
        d2 = [pscustomobject]@{
            textureSummaryPath = $textureSummaryPath
            texturePigs = @($pigTextures | ForEach-Object {
                    [pscustomobject]@{
                        pig = $_.pig
                        baseSha256 = $_.baseSha256
                        patchSha256 = $_.patchSha256
                        baseBitmapCount = $_.baseBitmapCount
                        patchBitmapCount = $_.patchBitmapCount
                        changed = $_.changed
                        extra = $_.extra
                        missing = $_.missing
                        textureCount = $_.textureCount
                        byIndexTextureCount = $_.byIndexTextureCount
                        texturePngBytes = $_.texturePngBytes
                    }
                })
            hamPatchFormat = "RFC 6902 JSON Patch"
            hamPatchTarget = "com.dxxredux.d2-ham-sections.v1"
            hamPatchPath = $hamDelta.path
            hamPatchSummaryPath = $hamDelta.summaryPath
            hamPatchOperationCount = $hamDelta.patchOperations
            hamDeltaCounts = [pscustomobject]@{
                textures = $hamDelta.textureDeltas
                vclips = $hamDelta.vclipDeltas
                eclips = $hamDelta.eclipDeltas
                wclips = $hamDelta.wclipDeltas
            }
            spCombinedAlignmentFieldDeltas = $hamDelta.spCombinedAlignmentDeltas
            skippedSources = @($skippedSources)
        }
    }
    Write-XfingJsonFile -Path (Join-Path $metadataRoot "manifest.json") -Value $manifest
    $outputPath = Join-Path $OutputDir "uud2tp-textures.dxa"
    New-XfingDxaFromDirectory -SourceDir $stageRoot -OutputPath $outputPath
    Write-Host "Created $outputPath"
    return [pscustomobject]@{
        path = $outputPath
        textures = (@($pigTextures) | Measure-Object textureCount -Sum).Sum
        byIndexTextures = (@($pigTextures) | Measure-Object byIndexTextureCount -Sum).Sum
        hamDeltas = $hamDelta.textureDeltas + $hamDelta.vclipDeltas + $hamDelta.eclipDeltas + $hamDelta.wclipDeltas
        skippedSources = @($skippedSources).Count
        size = (Get-Item -LiteralPath $outputPath).Length
    }
}

function Invoke-XfingStructuralPreflight {
    $preflightRoot = Join-Path $OutputDir ("preflight-{0}" -f [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Path $preflightRoot | Out-Null
    try {
        if ($Game -eq "d1" -or $Game -eq "both") {
            $d1Work = Join-Path $preflightRoot "d1"
            $baseHog = Join-Path $BaselineRoot "DESCENT.HOG"
            $null = Read-XfingD1Pig (Join-Path $BaselineRoot "DESCENT.PIG")
            $null = Read-XfingD1Pig (Join-Path $Uud1Root "descent.pig")
            $null = Get-XfingPaletteFromHog -HogPath $baseHog -EntryName "palette.256" -WorkDir $d1Work
            $baseLevelDir = Join-Path $d1Work "base_levels"
            foreach ($levelName in @("level03.rdl", "level17.rdl", "level20.rdl")) {
                $baseLevelPath = Join-Path $baseLevelDir $levelName
                Export-XfingHogEntry -HogPath $baseHog -EntryName $levelName -OutputPath $baseLevelPath
                $null = Read-XfingD1LevelSurfaces $baseLevelPath
                $null = Read-XfingD1LevelSurfaces (Join-Path $Uud1Root $levelName)
            }
        }

        if ($Game -eq "d2" -or $Game -eq "both") {
            $d2Work = Join-Path $preflightRoot "d2"
            $baseHog = Join-Path $BaselineRoot "DESCENT2.HOG"
            foreach ($pigName in @("GROUPA.PIG", "ALIEN1.PIG", "ALIEN2.PIG", "FIRE.PIG", "ICE.PIG", "WATER.PIG")) {
                $null = Read-XfingD2Pig (Join-Path $BaselineRoot $pigName)
                $null = Read-XfingD2Pig (Join-Path $Uud2Root $pigName)
                $paletteName = "$([System.IO.Path]::GetFileNameWithoutExtension($pigName).ToLowerInvariant()).256"
                $null = Get-XfingPaletteFromHog -HogPath $baseHog -EntryName $paletteName -WorkDir $d2Work
            }
            $patchedHamPath = Join-Path $d2Work "DESCENT2.HAM"
            Export-XfingZipEntry -ZipPath (Join-Path $Uud2Root "uud2tp.dxa") -EntryName "DESCENT2.HAM" -OutputPath $patchedHamPath
            $null = Read-XfingHamSections (Join-Path $BaselineRoot "DESCENT2.HAM")
            $null = Read-XfingHamSections $patchedHamPath
            if ($D2DescentBaselinePig -and (Test-Path -LiteralPath $D2DescentBaselinePig)) {
                $null = Read-XfingD2Pig $D2DescentBaselinePig
                $null = Read-XfingD2Pig (Join-Path $Uud2Root "DESCENT.PIG")
                $null = Read-XfingPaletteBytes (Join-Path $Uud2Root "descent.256")
            } elseif (Test-Path -LiteralPath (Join-Path $Uud2Root "DESCENT.PIG")) {
                $null = Read-XfingD2Pig (Join-Path $Uud2Root "DESCENT.PIG")
            }
        }
    } finally {
        Remove-Item -LiteralPath $preflightRoot -Recurse -Force
    }
}

Invoke-XfingStructuralPreflight

$results = @()
if ($Game -eq "d1" -or $Game -eq "both") {
    $results += New-XfingD1Archive
}
if ($Game -eq "d2" -or $Game -eq "both") {
    $results += New-XfingD2Archive
}

$results | Format-Table -AutoSize
