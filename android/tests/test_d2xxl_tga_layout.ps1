$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
. (Join-Path $repoRoot "game_data\mods\d2x-xl\convert_d2xxl_textures.ps1")

$tempRoot = Join-Path $repoRoot "temp\test_d2xxl_tga_layout"
if (Test-Path -LiteralPath $tempRoot) {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

function Set-TestUInt16 {
    param([byte[]]$Bytes, [int]$Offset, [int]$Value)

    [Array]::Copy([BitConverter]::GetBytes([uint16]$Value), 0, $Bytes, $Offset, 2)
}

function Set-TestUInt32 {
    param([byte[]]$Bytes, [int]$Offset, [long]$Value)

    [Array]::Copy([BitConverter]::GetBytes([uint32]$Value), 0, $Bytes, $Offset, 4)
}

function New-TestTga {
    param(
        [int]$Width = 2,
        [int]$Height = 2,
        [int]$BitsPerPixel = 24,
        [int]$Descriptor = 0x20,
        [object[]]$CanonicalPixels,
        [byte[]]$ImageId = @()
    )

    $header = [byte[]]::new(18)
    $header[0] = $ImageId.Length
    $header[2] = 2
    Set-TestUInt16 -Bytes $header -Offset 12 -Value $Width
    Set-TestUInt16 -Bytes $header -Offset 14 -Value $Height
    $header[16] = $BitsPerPixel
    $header[17] = $Descriptor

    $stored = [System.Collections.Generic.List[byte]]::new()
    $topOrigin = ($Descriptor -band 0x20) -ne 0
    $rightOrigin = ($Descriptor -band 0x10) -ne 0
    for ($storedY = 0; $storedY -lt $Height; $storedY++) {
        $sourceY = if ($topOrigin) { $storedY } else { $Height - 1 - $storedY }
        for ($storedX = 0; $storedX -lt $Width; $storedX++) {
            $sourceX = if ($rightOrigin) { $Width - 1 - $storedX } else { $storedX }
            [byte[]]$pixel = $CanonicalPixels[$sourceY * $Width + $sourceX]
            $stored.Add($pixel[0])
            $stored.Add($pixel[1])
            $stored.Add($pixel[2])
            if ($BitsPerPixel -eq 32) {
                $stored.Add($pixel[3])
            }
        }
    }
    return [byte[]]($header + $ImageId + $stored.ToArray())
}

function Add-TestTgaFooter {
    param(
        [byte[]]$Image,
        [switch]$ExtensionArea,
        [switch]$KeyInExtension,
        [switch]$DeveloperArea
    )

    $metadata = [byte[]]::new(0)
    $extensionOffset = 0
    if ($ExtensionArea) {
        $extensionOffset = $Image.Length
        $metadata = [byte[]]::new(495)
        Set-TestUInt16 -Bytes $metadata -Offset 0 -Value 495
        if ($KeyInExtension) {
            $metadata[10] = 128
            $metadata[11] = 88
            $metadata[12] = 120
        }
    }
    $developerOffset = 0
    if ($DeveloperArea) {
        $developerOffset = $Image.Length + $metadata.Length
        $directory = [byte[]]::new(15)
        Set-TestUInt16 -Bytes $directory -Offset 0 -Value 1
        Set-TestUInt16 -Bytes $directory -Offset 2 -Value 42
        Set-TestUInt32 -Bytes $directory -Offset 4 -Value ($developerOffset + 12)
        Set-TestUInt32 -Bytes $directory -Offset 8 -Value 3
        $directory[12] = 7
        $directory[13] = 8
        $directory[14] = 9
        $metadata = [byte[]]($metadata + $directory)
    }

    $footer = [byte[]]::new(26)
    Set-TestUInt32 -Bytes $footer -Offset 0 -Value $extensionOffset
    Set-TestUInt32 -Bytes $footer -Offset 4 -Value $developerOffset
    $signature = [System.Text.Encoding]::ASCII.GetBytes("TRUEVISION-XFILE." + [char]0)
    [Array]::Copy($signature, 0, $footer, 8, $signature.Length)
    return [byte[]]($Image + $metadata + $footer)
}

function Write-TestTga {
    param([string]$Name, [byte[]]$Bytes)

    $path = Join-Path $tempRoot $Name
    [System.IO.File]::WriteAllBytes($path, $Bytes)
    return $path
}

function Assert-TestPixel {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$X,
        [int]$Y,
        [byte[]]$Expected,
        [string]$Description
    )

    $actual = $Bitmap.GetPixel($X, $Y)
    if ($actual.B -ne $Expected[0] -or $actual.G -ne $Expected[1] -or
        $actual.R -ne $Expected[2] -or $actual.A -ne $Expected[3]) {
        throw "$Description was BGRA($($actual.B),$($actual.G),$($actual.R),$($actual.A))"
    }
}

function Assert-TestTgaRejected {
    param([string]$Name, [byte[]]$Bytes)

    try {
        $result = Read-TGA (Write-TestTga $Name $Bytes)
        if ($result) {
            $result.Bitmap.Dispose()
            if ($result.Mask) {
                $result.Mask.Dispose()
            }
        }
    } catch {
        return
    }
    throw "$Name was accepted"
}

$canonical = @(
    [byte[]](0, 0, 255, 255),
    [byte[]](0, 255, 0, 255),
    [byte[]](255, 0, 0, 255),
    [byte[]](0, 255, 255, 255)
)

foreach ($bitsPerPixel in @(24, 32)) {
    foreach ($origin in @(0x00, 0x10, 0x20, 0x30)) {
        $descriptor = $origin -bor $(if ($bitsPerPixel -eq 32) { 8 } else { 0 })
        $name = "origin-$bitsPerPixel-$('{0:x2}' -f $origin).tga"
        $result = Read-TGA (Write-TestTga $name (New-TestTga -BitsPerPixel $bitsPerPixel -Descriptor $descriptor -CanonicalPixels $canonical))
        try {
            for ($index = 0; $index -lt $canonical.Count; $index++) {
                Assert-TestPixel `
                    -Bitmap $result.Bitmap `
                    -X ($index % 2) `
                    -Y ([int][Math]::Floor($index / 2)) `
                    -Expected $canonical[$index] `
                    -Description "$name pixel $index"
            }
            if ($result.Mask) {
                throw "$name unexpectedly produced a mask"
            }
        } finally {
            $result.Bitmap.Dispose()
        }
    }
}

$idImage = New-TestTga -BitsPerPixel 24 -Descriptor 0x20 -CanonicalPixels $canonical -ImageId ([byte[]](1, 2, 3, 4))
$idImage[8] = 7
$idImage[10] = 9
$idResult = Read-TGA (Write-TestTga "image-id.tga" $idImage)
try {
    Assert-TestPixel -Bitmap $idResult.Bitmap -X 0 -Y 0 -Expected $canonical[0] -Description "image ID pixel"
} finally {
    $idResult.Bitmap.Dispose()
}

$footerImage = Add-TestTgaFooter -Image (New-TestTga -CanonicalPixels $canonical) -ExtensionArea -KeyInExtension
$footerResult = Read-TGA (Write-TestTga "extension-key.tga" $footerImage)
try {
    if ($footerResult.Mask) {
        throw "Key-like extension bytes produced a transparency mask"
    }
} finally {
    $footerResult.Bitmap.Dispose()
}

$immediateFooter = Add-TestTgaFooter -Image (New-TestTga -CanonicalPixels $canonical)
$immediateResult = Read-TGA (Write-TestTga "immediate-footer.tga" $immediateFooter)
$immediateResult.Bitmap.Dispose()

$developerImage = Add-TestTgaFooter -Image (New-TestTga -CanonicalPixels $canonical) -DeveloperArea
$developerResult = Read-TGA (Write-TestTga "developer-area.tga" $developerImage)
$developerResult.Bitmap.Dispose()

$keyPixels = @(
    [byte[]](128, 88, 120, 255),
    [byte[]](0, 0, 255, 255),
    [byte[]](255, 0, 0, 255),
    [byte[]](0, 255, 255, 255)
)
$keyResult = Read-TGA (Write-TestTga "key-color.tga" (New-TestTga -Descriptor 0x30 -CanonicalPixels $keyPixels))
try {
    if (-not $keyResult.Mask) {
        throw "Exact key pixel did not produce a mask"
    }
    if ($keyResult.Bitmap.GetPixel(0, 0).A -ne 0) {
        throw "Exact key pixel did not become transparent"
    }
    Assert-TestPixel -Bitmap $keyResult.Mask -X 0 -Y 0 -Expected ([byte[]](0, 0, 0, 0)) -Description "key mask"
    Assert-TestPixel -Bitmap $keyResult.Mask -X 1 -Y 0 -Expected ([byte[]](255, 255, 255, 255)) -Description "opaque mask"
} finally {
    $keyResult.Bitmap.Dispose()
    $keyResult.Mask.Dispose()
}

$alphaPixels = @(
    [byte[]](10, 20, 30, 0),
    [byte[]](40, 50, 60, 128),
    [byte[]](70, 80, 90, 255),
    [byte[]](100, 110, 120, 255)
)
$alphaResult = Read-TGA (Write-TestTga "alpha-eight.tga" (New-TestTga -BitsPerPixel 32 -Descriptor 0x28 -CanonicalPixels $alphaPixels))
try {
    if ($alphaResult.Bitmap.GetPixel(0, 0).A -ne 0 -or $alphaResult.Bitmap.GetPixel(1, 0).A -ne 128) {
        throw "Eight-bit alpha was not preserved"
    }
} finally {
    $alphaResult.Bitmap.Dispose()
}

$opaqueResult = Read-TGA (Write-TestTga "alpha-zero.tga" (New-TestTga -BitsPerPixel 32 -Descriptor 0x20 -CanonicalPixels $alphaPixels))
try {
    if ($opaqueResult.Bitmap.GetPixel(0, 0).A -ne 255 -or $opaqueResult.Bitmap.GetPixel(1, 0).A -ne 255) {
        throw "Zero descriptor alpha bits did not force opaque pixels"
    }
} finally {
    $opaqueResult.Bitmap.Dispose()
}

$valid = New-TestTga -CanonicalPixels $canonical
Assert-TestTgaRejected "truncated-header.tga" ([byte[]]$valid[0..10])
Assert-TestTgaRejected "truncated-pixels.tga" ([byte[]]$valid[0..($valid.Length - 2)])
$zeroWidth = [byte[]]$valid.Clone()
Set-TestUInt16 -Bytes $zeroWidth -Offset 12 -Value 0
Assert-TestTgaRejected "zero-width.tga" $zeroWidth
$extremeWidth = [byte[]]$valid.Clone()
Set-TestUInt16 -Bytes $extremeWidth -Offset 12 -Value 8193
Assert-TestTgaRejected "extreme-width.tga" $extremeWidth
$extremePixels = [byte[]]$valid.Clone()
Set-TestUInt16 -Bytes $extremePixels -Offset 12 -Value 8192
Set-TestUInt16 -Bytes $extremePixels -Offset 14 -Value 8192
Assert-TestTgaRejected "extreme-pixels.tga" $extremePixels
$mapped = [byte[]]$valid.Clone()
$mapped[1] = 1
Assert-TestTgaRejected "color-mapped.tga" $mapped
$contradictoryMap = [byte[]]$valid.Clone()
Set-TestUInt16 -Bytes $contradictoryMap -Offset 5 -Value 1
$contradictoryMap[7] = 24
Assert-TestTgaRejected "contradictory-color-map.tga" $contradictoryMap
$badDepth = [byte[]]$valid.Clone()
$badDepth[16] = 16
Assert-TestTgaRejected "bad-depth.tga" $badDepth
$bad24Alpha = [byte[]]$valid.Clone()
$bad24Alpha[17] = 0x21
Assert-TestTgaRejected "bad-24-alpha.tga" $bad24Alpha
$bad32Alpha = New-TestTga -BitsPerPixel 32 -Descriptor 0x21 -CanonicalPixels $canonical
Assert-TestTgaRejected "bad-32-alpha.tga" $bad32Alpha
$interleaved = [byte[]]$valid.Clone()
$interleaved[17] = 0x60
Assert-TestTgaRejected "interleaved.tga" $interleaved
Assert-TestTgaRejected "arbitrary-trailing.tga" ([byte[]]($valid + [byte[]](128, 88, 120)))
$badFooter = [byte[]](Add-TestTgaFooter -Image $valid)
$badFooter[$badFooter.Length - 1] = 1
Assert-TestTgaRejected "bad-footer.tga" $badFooter
$badExtension = [byte[]](Add-TestTgaFooter -Image $valid -ExtensionArea)
Set-TestUInt16 -Bytes $badExtension -Offset $valid.Length -Value 494
Assert-TestTgaRejected "bad-extension.tga" $badExtension
$badDeveloper = [byte[]](Add-TestTgaFooter -Image $valid -DeveloperArea)
Set-TestUInt32 -Bytes $badDeveloper -Offset ($valid.Length + 4) -Value 1
Assert-TestTgaRejected "bad-developer-tag.tga" $badDeveloper

$batchSource = Join-Path $tempRoot "batch-source"
$batchOutput = Join-Path $tempRoot "batch-output"
New-Item -ItemType Directory -Path $batchSource, $batchOutput -Force | Out-Null
$script:testTgaBatchSource = $batchSource
function Invoke-7ZipExtract {
    param([string]$SevenZipPath, [string]$ArchivePath, [string]$ExtractDir)

    $target = Join-Path $ExtractDir "textures\d1"
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item -Path (Join-Path $script:testTgaBatchSource "*.tga") -Destination $target
}

$fakeEtc2 = Join-Path $tempRoot "fake-etc2.cmd"
Set-Content -LiteralPath $fakeEtc2 -Value '@copy /y "%~1" "%~2" >nul' -Encoding ascii
$archivePath = Join-Path $tempRoot "fixture.7z"
Set-Content -LiteralPath $archivePath -Value "fixture" -Encoding ascii
Copy-Item -LiteralPath (Write-TestTga "batch-good.tga" $valid) -Destination (Join-Path $batchSource "good.tga") -Force
Convert-GameTextures -GameId "d1" -ArchivePath $archivePath -OutDir $batchOutput -MaxDim 0 -Readme "" -ProjectReadme "" -MagickPath "" -Etc2ToolPath $fakeEtc2
$batchDxa = Join-Path $batchOutput "d1-hires-512-textures-ktx2.dxa"
if (-not (Test-Path -LiteralPath $batchDxa)) {
    throw "Valid TGA batch did not publish its archive"
}

Copy-Item -LiteralPath (Write-TestTga "batch-bad.tga" ([byte[]]($valid + [byte]1))) -Destination (Join-Path $batchSource "bad.tga") -Force
try {
    Convert-GameTextures -GameId "d1" -ArchivePath $archivePath -OutDir $batchOutput -MaxDim 0 -Readme "" -ProjectReadme "" -MagickPath "" -Etc2ToolPath $fakeEtc2
    throw "Mixed valid and invalid TGA batch returned success"
} catch {
    if ($_.Exception.Message -eq "Mixed valid and invalid TGA batch returned success") {
        throw
    }
}
if (Test-Path -LiteralPath $batchDxa) {
    throw "Failed TGA batch retained a partial archive"
}

Write-Output "D2X-XL type-2 TGA layout tests passed"
