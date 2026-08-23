#!/usr/bin/env pwsh
param(
    [string]$Path = "",
    [string]$BaselineRoot = "",
    [string]$SourceDir = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..\..")
. (Join-Path $repoRoot "android/helpers/powershell_compat.ps1")
. (Join-Path $scriptDir "uud2sp_ham_patch_lib.ps1")

if (-not $Path) {
    $Path = Join-Path $scriptDir "UUD2SP1_4.no_ham.dxa"
}
if (-not $BaselineRoot) {
    $BaselineRoot = Join-Path $repoRoot "game_data_to_copy_to_emulator\temp"
}
if (-not $SourceDir) {
    $SourceDir = Join-Path $scriptDir "uud2sp"
}

function Read-Uud2spZipEntryBytes {
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

function Read-Uud2spZipEntryText {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$EntryName
    )

    $bytes = Read-Uud2spZipEntryBytes -Archive $Archive -EntryName $EntryName
    return [System.Text.Encoding]::UTF8.GetString($bytes)
}

function Assert-Uud2spEntrySha256 {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$EntryName,
        [string]$ExpectedSha256
    )

    $bytes = Read-Uud2spZipEntryBytes -Archive $Archive -EntryName $EntryName
    $actual = Get-XfingSha256ForBytes -Bytes $bytes
    if ($actual -ne $ExpectedSha256) {
        throw "Hash mismatch for $EntryName`: expected $ExpectedSha256, actual $actual"
    }
    return $bytes.Length
}

function Assert-Uud2spBytesEqual {
    param([byte[]]$Left, [byte[]]$Right, [int]$Length, [string]$Description)

    $difference = Get-Uud2spFirstByteDifference -Left $Left -Right $Right -Length $Length
    if ($difference -ge 0) {
        throw "$Description differs at byte $difference"
    }
}

if (-not (Test-Path -LiteralPath $Path)) {
    throw "Missing DXA: $Path"
}

$baseHamPath = Join-Path $BaselineRoot "DESCENT2.HAM"
$patchedHamPath = Join-Path $SourceDir "descent2.ham"
foreach ($requiredPath in @($baseHamPath, $patchedHamPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Missing required input: $requiredPath"
    }
}

$archive = [System.IO.Compression.ZipFile]::OpenRead($Path)
try {
    $hamEntries = @($archive.Entries | Where-Object { [System.IO.Path]::GetExtension($_.FullName).ToLowerInvariant() -eq ".ham" })
    if ($hamEntries.Count -gt 0) {
        throw "Forbidden HAM entries found: $(($hamEntries | ForEach-Object FullName) -join ', ')"
    }

    $manifest = (Read-Uud2spZipEntryText -Archive $archive -EntryName "metadata/manifest.json") | ConvertFrom-Json
    if ($manifest.schema -ne "com.dxxredux.sound-patch-dxa.v1") {
        throw "Unexpected manifest schema: $($manifest.schema)"
    }
    if ($manifest.pack -ne "uud2sp" -or $manifest.game -ne "d2") {
        throw "Unexpected manifest pack/game: $($manifest.pack)/$($manifest.game)"
    }

    $patchOps = @(ConvertFrom-CompatibleJsonItems -Json (Read-Uud2spZipEntryText -Archive $archive -EntryName $manifest.d2.hamPatchPath))
    $summary = (Read-Uud2spZipEntryText -Archive $archive -EntryName $manifest.d2.hamPatchSummaryPath) | ConvertFrom-Json
    if ($patchOps.Count -ne [int]$manifest.d2.hamPatchOperationCount) {
        throw "Manifest operation count does not match patch file"
    }
    if ($patchOps.Count -ne [int]$summary.operationCount) {
        throw "Summary operation count does not match patch file"
    }

    foreach ($op in $patchOps) {
        if (@("test", "replace") -notcontains [string]$op.op) {
            throw "Unexpected HAM patch operation: $($op.op)"
        }
        if (-not ($op.PSObject.Properties.Name -contains "value")) {
            throw "HAM patch operation is missing value: $($op.path)"
        }
        $null = Split-Uud2spPatchPath $op.path
    }

    $baseBytes = [IO.File]::ReadAllBytes($baseHamPath)
    $patchedBytes = [IO.File]::ReadAllBytes($patchedHamPath)
    $expectedBytes = Copy-Uud2spBytes $patchedBytes
    $expectedLayout = Get-Uud2spHamLayout $expectedBytes
    if ($summary.PSObject.Properties.Name -contains "ignoredFieldDeltas") {
        foreach ($row in @($summary.ignoredFieldDeltas)) {
            Write-Uud2spHamFieldValue -Bytes $expectedBytes -Layout $expectedLayout -Section $row.section -Index $row.index -Field $row.field -Value ([int]$row.base)
        }
    }
    if ($summary.PSObject.Properties.Name -contains "combinedAlignmentFieldDeltas") {
        foreach ($row in @($summary.combinedAlignmentFieldDeltas)) {
            Write-Uud2spHamFieldValue -Bytes $expectedBytes -Layout $expectedLayout -Section $row.section -Index $row.index -Field $row.field -Value ([int]$row.patch)
        }
    }
    $generatedBytes = Apply-Uud2spHamPatchOperations -BaseBytes $baseBytes -PatchOperations $patchOps
    Assert-Uud2spBytesEqual -Left $generatedBytes -Right $expectedBytes -Length $baseBytes.Length -Description "Generated normalized HAM retail-length prefix"
    if ([long]$summary.bitmapXlatOffset + ([long]$summary.bitmapXlatCount * 2) -ne [long]$summary.engineComparableLength) {
        throw "Summary GameBitmapXlat span does not match comparable HAM length"
    }

    $generatedSha256 = Get-XfingSha256ForBytes -Bytes $generatedBytes
    if ($generatedSha256 -ne $summary.generatedComparableSha256) {
        throw "Generated HAM SHA-256 does not match summary"
    }
    $patchedComparableSha256 = Get-XfingSha256ForBytes -Bytes $expectedBytes -Offset 0 -Length $baseBytes.Length
    if ($patchedComparableSha256 -ne $summary.engineComparableSha256) {
        throw "Normalized patched HAM comparable SHA-256 does not match summary"
    }
    if ($summary.originalEngineComparableSha256) {
        $originalComparableSha256 = Get-XfingSha256ForBytes -Bytes $patchedBytes -Offset 0 -Length $baseBytes.Length
        if ($originalComparableSha256 -ne $summary.originalEngineComparableSha256) {
            throw "Original patched HAM comparable SHA-256 does not match summary"
        }
    }
    $ignoredTrailerLength = $patchedBytes.Length - $baseBytes.Length
    if ($ignoredTrailerLength -ne [int]$summary.ignoredOriginalTrailer.size) {
        throw "Ignored trailer size does not match summary"
    }
    if ($ignoredTrailerLength -gt 0) {
        $ignoredTrailerSha256 = Get-XfingSha256ForBytes -Bytes $patchedBytes -Offset $baseBytes.Length -Length $ignoredTrailerLength
        if ($ignoredTrailerSha256 -ne $summary.ignoredOriginalTrailer.sha256) {
            throw "Ignored trailer SHA-256 does not match summary"
        }
    }

    $checkedSoundBytes = 0
    foreach ($soundFile in @($manifest.d2.soundFiles)) {
        $checkedSoundBytes += Assert-Uud2spEntrySha256 -Archive $archive -EntryName $soundFile.path -ExpectedSha256 $soundFile.sha256
    }
    $null = Read-Uud2spZipEntryBytes -Archive $archive -EntryName "UUD2SP.rtf"
    $null = Read-Uud2spZipEntryBytes -Archive $archive -EntryName "README.md"

    [pscustomobject]@{
        path = $Path
        entries = $archive.Entries.Count
        archiveBytes = (Get-Item -LiteralPath $Path).Length
        hamEntries = $hamEntries.Count
        hamPatchOperations = $patchOps.Count
        hamFieldDeltas = [int]$summary.fieldDeltaCount
        ignoredHamFieldDeltas = if ($summary.PSObject.Properties.Name -contains "ignoredFieldDeltaCount") { [int]$summary.ignoredFieldDeltaCount } else { 0 }
        combinedAlignmentHamFieldDeltas = if ($summary.PSObject.Properties.Name -contains "combinedAlignmentFieldDeltaCount") { [int]$summary.combinedAlignmentFieldDeltaCount } else { 0 }
        generatedComparableSha256 = $generatedSha256
        originalPatchedHamSha256 = Get-XfingSha256ForBytes -Bytes $patchedBytes
        ignoredOriginalHamTrailerBytes = $ignoredTrailerLength
        soundBytes = $checkedSoundBytes
    }
} finally {
    $archive.Dispose()
}
