param(
    [string[]]$Path
)

$ErrorActionPreference = "Stop"
$scriptDir = $PSScriptRoot
. (Join-Path $scriptDir "xfing_minimal_dxa_lib.ps1")

if (-not $Path -or $Path.Count -eq 0) {
    $defaultDir = Join-Path $scriptDir "dxx_tp\tmp\plain_texture_dxa"
    $Path = @(Get-ChildItem -LiteralPath $defaultDir -Filter "*-textures.dxa" -File -ErrorAction SilentlyContinue | ForEach-Object FullName)
}

if (-not $Path -or $Path.Count -eq 0) {
    throw "No DXA paths supplied and no default generated DXAs found"
}

function Read-XfingZipEntryText {
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

function Get-XfingZipEntrySha256 {
    param([System.IO.Compression.ZipArchiveEntry]$Entry)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $stream = $Entry.Open()
        try {
            return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace("-", "").ToLowerInvariant()
        } finally {
            $stream.Dispose()
        }
    } finally {
        $sha.Dispose()
    }
}

function Test-XfingPngSignature {
    param([System.IO.Compression.ZipArchiveEntry]$Entry)

    $expected = [byte[]](0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a)
    $actual = [byte[]]::new($expected.Length)
    $stream = $Entry.Open()
    try {
        $read = $stream.Read($actual, 0, $actual.Length)
        if ($read -ne $expected.Length) {
            return $false
        }
    } finally {
        $stream.Dispose()
    }
    for ($index = 0; $index -lt $expected.Length; $index++) {
        if ($actual[$index] -ne $expected[$index]) {
            return $false
        }
    }
    return $true
}

function Assert-XfingPngEntry {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$EntryName,
        [string]$Sha256,
        [string]$DxaPath
    )

    $entry = $Archive.GetEntry($EntryName)
    if (-not $entry) {
        throw "Missing PNG $EntryName in $DxaPath"
    }
    if ([System.IO.Path]::GetExtension($EntryName).ToLowerInvariant() -ne ".png") {
        throw "Texture entry is not a PNG: $EntryName"
    }
    if (-not (Test-XfingPngSignature -Entry $entry)) {
        throw "Invalid PNG signature for $EntryName"
    }
    $hash = Get-XfingZipEntrySha256 -Entry $entry
    if ($hash -ne $Sha256) {
        throw "Hash mismatch for $EntryName"
    }
    return $entry.Length
}

function Assert-XfingJsonPatchEntry {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        [string]$EntryName,
        [string]$DxaPath
    )

    if (-not $EntryName) {
        return 0
    }
    $entry = $Archive.GetEntry($EntryName)
    if (-not $entry) {
        throw "Missing JSON Patch $EntryName in $DxaPath"
    }
    $ops = @(Read-XfingZipEntryText -Archive $Archive -EntryName $EntryName | ConvertFrom-Json)
    foreach ($op in $ops) {
        if (-not $op.op -or -not $op.path) {
            throw "Invalid JSON Patch operation in $EntryName"
        }
        if (@("add", "remove", "replace", "move", "copy", "test") -notcontains $op.op) {
            throw "Unsupported JSON Patch op '$($op.op)' in $EntryName"
        }
        if (-not ([string]$op.path).StartsWith("/")) {
            throw "Invalid JSON Patch path '$($op.path)' in $EntryName"
        }
        if (@("add", "replace", "test") -contains $op.op -and -not ($op.PSObject.Properties.Name -contains "value")) {
            throw "JSON Patch op '$($op.op)' is missing value in $EntryName"
        }
    }
    return @($ops).Count
}

function Get-XfingManifestTextures {
    param(
        [System.IO.Compression.ZipArchive]$Archive,
        $Manifest
    )

    $textures = @()
    if ($Manifest.d1 -and $Manifest.d1.texturePigs) {
        foreach ($texturePig in @($Manifest.d1.texturePigs)) {
            if ($texturePig.textures) {
                $textures += @($texturePig.textures)
            }
        }
    }
    if ($Manifest.d2 -and $Manifest.d2.textureSummaryPath) {
        $summaryText = Read-XfingZipEntryText -Archive $Archive -EntryName $Manifest.d2.textureSummaryPath
        foreach ($texturePig in @($summaryText | ConvertFrom-Json)) {
            if ($texturePig.textures) {
                $textures += @($texturePig.textures)
            }
        }
    }
    return @($textures)
}

function Assert-XfingCompatibilityMetadata {
    param(
        $Manifest,
        [string]$DxaPath
    )

    if (-not $Manifest.compatibility) {
        throw "Missing compatibility metadata in $DxaPath"
    }
    if (-not $Manifest.compatibility.requiredBaseDescription) {
        throw "Missing required base description in $DxaPath"
    }
    $requiredNote = @($Manifest.notes) | Where-Object { $_ -like "Required base files:*" } | Select-Object -First 1
    if (-not $requiredNote) {
        throw "Missing required base files note in $DxaPath"
    }
    $requiredFiles = @($Manifest.compatibility.requiredBaseFiles)
    if ($requiredFiles.Count -eq 0) {
        throw "Missing required base files list in $DxaPath"
    }
    foreach ($requiredFile in $requiredFiles) {
        foreach ($property in @("game", "filename", "sha256", "size", "version", "reason")) {
            if (-not ($requiredFile.PSObject.Properties.Name -contains $property) -or -not $requiredFile.$property) {
                throw "Required base file entry is missing $property in $DxaPath"
            }
        }
        if ($requiredFile.sha256 -notmatch "^[0-9a-f]{64}$") {
            throw "Invalid required base SHA-256 for $($requiredFile.filename) in $DxaPath"
        }
        if ([long]$requiredFile.size -le 0) {
            throw "Invalid required base size for $($requiredFile.filename) in $DxaPath"
        }
    }
    return $requiredFiles.Count
}

function Test-XfingArchive {
    param([string]$DxaPath)

    if (-not (Test-Path -LiteralPath $DxaPath)) {
        throw "Missing DXA: $DxaPath"
    }

    $archive = [System.IO.Compression.ZipFile]::OpenRead($DxaPath)
    try {
        $forbiddenExtensions = @(".pig", ".ham", ".hog", ".mn2", ".rdl", ".rl2", ".256", ".bin")
        $forbiddenEntries = @($archive.Entries | Where-Object {
                $extension = [System.IO.Path]::GetExtension($_.FullName).ToLowerInvariant()
                $forbiddenExtensions -contains $extension
            })
        if ($forbiddenEntries.Count -gt 0) {
            $names = ($forbiddenEntries | ForEach-Object FullName) -join ", "
            throw "Forbidden entries found in $DxaPath`: $names"
        }

        $manifest = (Read-XfingZipEntryText -Archive $archive -EntryName "metadata/manifest.json") | ConvertFrom-Json
        if ($manifest.schema -ne "com.dxxredux.plain-texture-dxa.v1") {
            throw "Unexpected schema in $DxaPath`: $($manifest.schema)"
        }
        $requiredBaseFileCount = Assert-XfingCompatibilityMetadata -Manifest $manifest -DxaPath $DxaPath

        $textures = Get-XfingManifestTextures -Archive $archive -Manifest $manifest
        $checkedBytes = 0
        foreach ($texture in $textures) {
            $checkedBytes += Assert-XfingPngEntry -Archive $archive -EntryName $texture.path -Sha256 $texture.pngSha256 -DxaPath $DxaPath
            if ($texture.maskPath) {
                $checkedBytes += Assert-XfingPngEntry -Archive $archive -EntryName $texture.maskPath -Sha256 $texture.maskSha256 -DxaPath $DxaPath
            }
        }

        if ($manifest.d1) {
            $null = Assert-XfingJsonPatchEntry -Archive $archive -EntryName $manifest.d1.levelSurfacePatchPath -DxaPath $DxaPath
            foreach ($extraPath in @($manifest.d1.levelSurfacePatchSummaryPath)) {
                if ($extraPath -and -not $archive.GetEntry($extraPath)) {
                    throw "Missing D1 detail file $extraPath"
                }
            }
        }
        if ($manifest.d2) {
            $null = Assert-XfingJsonPatchEntry -Archive $archive -EntryName $manifest.d2.hamPatchPath -DxaPath $DxaPath
            foreach ($extraPath in @($manifest.d2.textureSummaryPath, $manifest.d2.hamPatchSummaryPath)) {
                if ($extraPath -and -not $archive.GetEntry($extraPath)) {
                    throw "Missing D2 detail file $extraPath"
                }
            }
        }

        return [pscustomobject]@{
            path = $DxaPath
            pack = $manifest.pack
            game = $manifest.game
            entries = $archive.Entries.Count
            textures = @($textures).Count
            textureBytes = $checkedBytes
            archiveBytes = (Get-Item -LiteralPath $DxaPath).Length
            forbiddenEntries = $forbiddenEntries.Count
            requiredBaseFiles = $requiredBaseFileCount
        }
    } finally {
        $archive.Dispose()
    }
}

$results = @()
foreach ($dxaPath in $Path) {
    $results += Test-XfingArchive -DxaPath $dxaPath
}

$results | Format-Table -AutoSize
Write-Host "Verified $($results.Count) DXA archives"
