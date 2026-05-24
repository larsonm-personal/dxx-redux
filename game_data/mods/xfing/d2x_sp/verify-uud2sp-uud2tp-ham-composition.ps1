#!/usr/bin/env pwsh
param(
    [string]$BaselineRoot = "",
    [string]$SoundDxaPath = "",
    [string]$TextureDxaPath = "",
    [string]$CombinedDxaPath = "",
    [string]$ExpectedSemanticSha256 = "0d150357c459916ebe74dab8a578c40f509b7208367b3f60cb421dc76ed91a03"
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..\..")
. (Join-Path $scriptDir "uud2sp_ham_patch_lib.ps1")

if (-not $BaselineRoot) {
    $BaselineRoot = Join-Path $repoRoot "game_data_to_copy_to_emulator\temp"
}
if (-not $SoundDxaPath) {
    $SoundDxaPath = Join-Path $scriptDir "UUD2SP1_4.no_ham.dxa"
}
if (-not $TextureDxaPath) {
    $TextureDxaPath = Join-Path $repoRoot "game_data\mods\xfing\dxx_tp\tmp\plain_texture_dxa\uud2tp-textures.dxa"
}
if (-not $CombinedDxaPath) {
    $CombinedDxaPath = Join-Path $repoRoot "game_data\mods\xfing\dxx_tp\d2tp_sp_combined\UUD2T&SP1_4.dxa"
}

function Read-Uud2CompositionZipEntryText {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$EntryName
    )

    $entry = $Archive.GetEntry($EntryName)
    if (-not $entry) {
        throw "Missing $EntryName"
    }
    $stream = $entry.Open()
    try {
        $reader = [System.IO.StreamReader]::new($stream)
        try {
            return $reader.ReadToEnd()
        } finally {
            $reader.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Read-Uud2CompositionZipEntryBytes {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$EntryName
    )

    $entry = $Archive.GetEntry($EntryName)
    if (-not $entry) {
        throw "Missing $EntryName"
    }
    $stream = $entry.Open()
    try {
        $memory = [System.IO.MemoryStream]::new()
        try {
            $stream.CopyTo($memory)
            Write-Output -InputObject $memory.ToArray() -NoEnumerate
        } finally {
            $memory.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Expand-Uud2CompositionCombinedHam {
    param([string]$Path)

    if (-not $Path) {
        return ""
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing combined DXA: $Path"
    }
    $archive = [System.IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $bytes = Read-Uud2CompositionZipEntryBytes -Archive $archive -EntryName "DESCENT2.HAM"
    } finally {
        $archive.Dispose()
    }
    $outputDir = Join-Path $repoRoot "temp\d2tp_sp_combined_composition"
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    $outputPath = Join-Path $outputDir "DESCENT2.HAM"
    [System.IO.File]::WriteAllBytes($outputPath, $bytes)
    return $outputPath
}

function Read-Uud2CompositionPatchDocument {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing DXA: $Path"
    }
    $archive = [System.IO.Compression.ZipFile]::OpenRead($Path)
    try {
        $manifest = Read-Uud2CompositionZipEntryText -Archive $archive -EntryName "metadata/manifest.json" | ConvertFrom-Json
        $patchPath = [string]$manifest.d2.hamPatchPath
        if (-not $patchPath) {
            throw "Missing d2.hamPatchPath in $Path"
        }
        return [pscustomobject]@{
            path = $Path
            patchPath = $patchPath
            operations = @((Read-Uud2CompositionZipEntryText -Archive $archive -EntryName $patchPath) | ConvertFrom-Json)
        }
    } finally {
        $archive.Dispose()
    }
}

function ConvertTo-Uud2CompositionCanonicalJson {
    param($Value)

    if ($null -eq $Value) {
        return "null"
    }
    if ($Value -is [string] -or $Value -is [bool] -or $Value -is [ValueType]) {
        return ConvertTo-Json -InputObject $Value -Compress -Depth 20
    }
    if ($Value -is [System.Collections.IEnumerable] -and -not ($Value -is [string]) -and -not ($Value -is [pscustomobject])) {
        $items = @($Value | ForEach-Object { ConvertTo-Uud2CompositionCanonicalJson $_ })
        return "[$($items -join ',')]"
    }
    $parts = @()
    foreach ($property in @($Value.PSObject.Properties | Sort-Object Name)) {
        $name = ConvertTo-Json -InputObject $property.Name -Compress
        $parts += "$name`:$(ConvertTo-Uud2CompositionCanonicalJson $property.Value)"
    }
    return "{$($parts -join ',')}"
}

function Split-Uud2CompositionScope {
    param([string]$Path)

    if ($Path -notmatch '^(/sections/[^/]+/[0-9]+)(?:/([^/]+))?$') {
        throw "Unsupported patch path: $Path"
    }
    return [pscustomobject]@{ Row = $Matches[1]; Field = $Matches[2] }
}

function Test-Uud2CompositionScopeOverlap {
    param([string]$Left, [string]$Right)

    $leftScope = Split-Uud2CompositionScope $Left
    $rightScope = Split-Uud2CompositionScope $Right
    return $leftScope.Row -eq $rightScope.Row -and (-not $leftScope.Field -or -not $rightScope.Field -or $leftScope.Field -eq $rightScope.Field)
}

function Get-Uud2CompositionRowValue {
    param($Sections, [string]$Section, [int]$Index)

    switch ($Section) {
        "textures" { return $Sections.Textures[$Index] }
        "vclips" { return $Sections.Vclips[$Index] }
        "eclips" { return $Sections.Eclips[$Index] }
        "wclips" { return $Sections.Walls[$Index] }
    }
    throw "Unsupported row patch section: $Section"
}

function Get-Uud2CompositionPatchValue {
    param([byte[]]$BaseBytes, $BaseLayout, $BaseSections, [string]$Path)

    if ($Path -notmatch '^/sections/([^/]+)/([0-9]+)(?:/([^/]+))?$') {
        throw "Unsupported patch path: $Path"
    }
    $section = $Matches[1]
    $index = [int]$Matches[2]
    $field = $Matches[3]
    if (-not $field) {
        return Get-Uud2CompositionRowValue -Sections $BaseSections -Section $section -Index $index
    }
    if (@("textures", "vclips", "eclips", "wclips") -contains $section) {
        $row = Get-Uud2CompositionRowValue -Sections $BaseSections -Section $section -Index $index
        return $row.$field
    }
    return Read-Uud2spHamFieldValue -Bytes $BaseBytes -Layout $BaseLayout -Section $section -Index $index -Field $field
}

function Assert-Uud2CompositionPatchTests {
    param([byte[]]$BaseBytes, $BaseLayout, $BaseSections, $Document)

    foreach ($op in @($Document.operations)) {
        if ([string]$op.op -ne "test") {
            continue
        }
        $actual = Get-Uud2CompositionPatchValue -BaseBytes $BaseBytes -BaseLayout $BaseLayout -BaseSections $BaseSections -Path ([string]$op.path)
        if ((ConvertTo-Uud2CompositionCanonicalJson $actual) -ne (ConvertTo-Uud2CompositionCanonicalJson $op.value)) {
            throw "Patch test failed for $($Document.path) $($op.path)"
        }
    }
}

function Get-Uud2CompositionWrites {
    param($Document)

    $tests = @{}
    $writes = @()
    foreach ($op in @($Document.operations)) {
        $opName = [string]$op.op
        $path = [string]$op.path
        if ($opName -eq "test") {
            $tests[$path] = $op.value
            continue
        }
        if (@("add", "replace") -notcontains $opName) {
            throw "Unsupported patch operation $opName in $($Document.path)"
        }
        if ($opName -eq "replace" -and $tests.ContainsKey($path) -and $op.value -is [pscustomobject] -and $tests[$path] -is [pscustomobject]) {
            $names = @($op.value.PSObject.Properties.Name + $tests[$path].PSObject.Properties.Name | Sort-Object -Unique)
            foreach ($name in $names) {
                $baseValue = if ($tests[$path].PSObject.Properties.Name -contains $name) { $tests[$path].$name } else { $null }
                $patchValue = if ($op.value.PSObject.Properties.Name -contains $name) { $op.value.$name } else { $null }
                if ((ConvertTo-Uud2CompositionCanonicalJson $baseValue) -ne (ConvertTo-Uud2CompositionCanonicalJson $patchValue)) {
                    $writes += [pscustomobject]@{ Scope = "$path/$name"; Value = $patchValue; Source = $Document.path }
                }
            }
        } else {
            $writes += [pscustomobject]@{ Scope = $path; Value = $op.value; Source = $Document.path }
        }
    }
    return @($writes)
}

function Get-Uud2CompositionFieldWrites {
    param($Document)

    $writes = @()
    foreach ($op in @($Document.operations)) {
        $opName = [string]$op.op
        if ($opName -eq "test") {
            continue
        }
        if (@("add", "replace") -notcontains $opName) {
            throw "Unsupported patch operation $opName in $($Document.path)"
        }
        $path = [string]$op.path
        if ($op.value -is [pscustomobject] -and -not (Split-Uud2CompositionScope $path).Field) {
            foreach ($property in @($op.value.PSObject.Properties)) {
                $writes += [pscustomobject]@{ Scope = "$path/$($property.Name)"; Value = $property.Value; Source = $Document.path }
            }
        } else {
            $writes += [pscustomobject]@{ Scope = $path; Value = $op.value; Source = $Document.path }
        }
    }
    return @($writes)
}

function Merge-Uud2CompositionWrites {
    param([object[]]$Documents)

    $merged = [ordered]@{}
    foreach ($document in $Documents) {
        foreach ($write in @(Get-Uud2CompositionWrites $document)) {
            foreach ($existingScope in @($merged.Keys)) {
                if (-not (Test-Uud2CompositionScopeOverlap -Left $existingScope -Right $write.Scope)) {
                    continue
                }
                if ((ConvertTo-Uud2CompositionCanonicalJson $merged[$existingScope].Value) -ne (ConvertTo-Uud2CompositionCanonicalJson $write.Value)) {
                    throw "Overlapping patch writes: $existingScope and $($write.Scope)"
                }
            }
            $merged[$write.Scope] = $write
        }
    }
    return $merged
}

function Get-Uud2CompositionStateHash {
    param($Merged)

    $lines = foreach ($scope in @($Merged.Keys | Sort-Object)) {
        "$scope=$(ConvertTo-Uud2CompositionCanonicalJson $Merged[$scope].Value)"
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    return Get-XfingSha256ForBytes -Bytes $bytes
}

function Get-Uud2CompositionBaseValueOrNull {
    param([byte[]]$BaseBytes, $BaseLayout, $BaseSections, [string]$Path)

    try {
        return Get-Uud2CompositionPatchValue -BaseBytes $BaseBytes -BaseLayout $BaseLayout -BaseSections $BaseSections -Path $Path
    } catch {
        return $null
    }
}

function Apply-Uud2CompositionDocuments {
    param([byte[]]$BaseBytes, $BaseLayout, $BaseSections, [object[]]$Documents)

    $baseState = @{}
    $state = @{}
    foreach ($document in $Documents) {
        foreach ($write in @(Get-Uud2CompositionFieldWrites $document)) {
            if (-not $baseState.ContainsKey($write.Scope)) {
                $baseState[$write.Scope] = Get-Uud2CompositionBaseValueOrNull -BaseBytes $BaseBytes -BaseLayout $BaseLayout -BaseSections $BaseSections -Path $write.Scope
                $state[$write.Scope] = $baseState[$write.Scope]
            }
            $state[$write.Scope] = $write.Value
        }
    }
    return [pscustomobject]@{ Base = $baseState; State = $state }
}

function Get-Uud2CompositionChangedStateHash {
    param($Applied)

    $lines = foreach ($scope in @($Applied.State.Keys | Sort-Object)) {
        if ((ConvertTo-Uud2CompositionCanonicalJson $Applied.Base[$scope]) -eq (ConvertTo-Uud2CompositionCanonicalJson $Applied.State[$scope])) {
            continue
        }
        "$scope=$(ConvertTo-Uud2CompositionCanonicalJson $Applied.State[$scope])"
    }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes(($lines -join "`n"))
    return Get-XfingSha256ForBytes -Bytes $bytes
}

function Get-Uud2CompositionChangedScopeCount {
    param($Applied)

    return @($Applied.State.Keys | Where-Object { (ConvertTo-Uud2CompositionCanonicalJson $Applied.Base[$_]) -ne (ConvertTo-Uud2CompositionCanonicalJson $Applied.State[$_]) }).Count
}

function Get-Uud2CompositionSharedFieldScopes {
    param([object[]]$Documents)

    $counts = @{}
    foreach ($document in $Documents) {
        $scopes = @(Get-Uud2CompositionFieldWrites $document | ForEach-Object { $_.Scope } | Sort-Object -Unique)
        foreach ($scope in $scopes) {
            $counts[$scope] = 1 + $(if ($counts.ContainsKey($scope)) { $counts[$scope] } else { 0 })
        }
    }
    return @($counts.Keys | Where-Object { $counts[$_] -gt 1 } | Sort-Object)
}

function Assert-Uud2CompositionMatchesCombined {
    param([byte[]]$CombinedBytes, $CombinedLayout, $CombinedSections, $Applied, [string[]]$Scopes)

    foreach ($scope in $Scopes) {
        $combinedValue = Get-Uud2CompositionPatchValue -BaseBytes $CombinedBytes -BaseLayout $CombinedLayout -BaseSections $CombinedSections -Path $scope
        if ((ConvertTo-Uud2CompositionCanonicalJson $Applied.State[$scope]) -ne (ConvertTo-Uud2CompositionCanonicalJson $combinedValue)) {
            throw "Combined HAM mismatch at $scope"
        }
    }
}

$baseHamPath = Join-Path $BaselineRoot "DESCENT2.HAM"
if (-not (Test-Path -LiteralPath $baseHamPath)) {
    throw "Missing baseline HAM: $baseHamPath"
}

$baseBytes = [IO.File]::ReadAllBytes($baseHamPath)
$baseLayout = Get-Uud2spHamLayout $baseBytes
$baseSections = Read-XfingHamSections $baseHamPath
$soundDocument = Read-Uud2CompositionPatchDocument $SoundDxaPath
$textureDocument = Read-Uud2CompositionPatchDocument $TextureDxaPath
$combinedHamPath = Expand-Uud2CompositionCombinedHam $CombinedDxaPath
$combinedBytes = if ($combinedHamPath) { [IO.File]::ReadAllBytes($combinedHamPath) } else { $null }
$combinedLayout = if ($combinedHamPath) { Get-Uud2spHamLayout $combinedBytes } else { $null }
$combinedSections = if ($combinedHamPath) { Read-XfingHamSections $combinedHamPath } else { $null }

Assert-Uud2CompositionPatchTests -BaseBytes $baseBytes -BaseLayout $baseLayout -BaseSections $baseSections -Document $soundDocument
Assert-Uud2CompositionPatchTests -BaseBytes $baseBytes -BaseLayout $baseLayout -BaseSections $baseSections -Document $textureDocument

$soundThenTexture = Merge-Uud2CompositionWrites @($soundDocument, $textureDocument)
$textureThenSound = Merge-Uud2CompositionWrites @($textureDocument, $soundDocument)
$sharedScopes = Get-Uud2CompositionSharedFieldScopes @($soundDocument, $textureDocument)
$appliedSoundThenTexture = Apply-Uud2CompositionDocuments -BaseBytes $baseBytes -BaseLayout $baseLayout -BaseSections $baseSections -Documents @($soundDocument, $textureDocument)
$appliedTextureThenSound = Apply-Uud2CompositionDocuments -BaseBytes $baseBytes -BaseLayout $baseLayout -BaseSections $baseSections -Documents @($textureDocument, $soundDocument)
$soundThenTextureHash = Get-Uud2CompositionChangedStateHash $appliedSoundThenTexture
$textureThenSoundHash = Get-Uud2CompositionChangedStateHash $appliedTextureThenSound
if ($soundThenTextureHash -ne $textureThenSoundHash) {
    throw "Patch order changed the semantic HAM hash: $soundThenTextureHash vs $textureThenSoundHash"
}
if ($combinedHamPath) {
    Assert-Uud2CompositionMatchesCombined -CombinedBytes $combinedBytes -CombinedLayout $combinedLayout -CombinedSections $combinedSections -Applied $appliedSoundThenTexture -Scopes $sharedScopes
    Assert-Uud2CompositionMatchesCombined -CombinedBytes $combinedBytes -CombinedLayout $combinedLayout -CombinedSections $combinedSections -Applied $appliedTextureThenSound -Scopes $sharedScopes
}
if ($ExpectedSemanticSha256 -and $soundThenTextureHash -ne $ExpectedSemanticSha256) {
    throw "Combined semantic HAM hash mismatch: expected $ExpectedSemanticSha256 actual $soundThenTextureHash"
}

[pscustomobject]@{
    baselineHam = $baseHamPath
    soundDxa = $SoundDxaPath
    textureDxa = $TextureDxaPath
    combinedDxa = $CombinedDxaPath
    soundPatchOperations = @($soundDocument.operations).Count
    texturePatchOperations = @($textureDocument.operations).Count
    overlapWriteScopes = @($soundThenTexture.Keys).Count
    sharedFieldScopes = @($sharedScopes).Count
    combinedWriteScopes = Get-Uud2CompositionChangedScopeCount $appliedSoundThenTexture
    semanticSha256 = $soundThenTextureHash
    expectedSemanticSha256 = $ExpectedSemanticSha256
}