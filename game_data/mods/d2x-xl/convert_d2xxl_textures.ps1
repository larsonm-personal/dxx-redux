#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Converts d2x-xl high-res TGA texture packs into .dxa (ZIP) files
    loadable by the DXX-Rebirth/Redux engine.

.DESCRIPTION
    Extracts TGA files from the d2x-xl 7z archives, compresses them to
    ETC2 format via etc2tool, and packages the .ktx2 files into .dxa
    files (ZIP archives). The engine auto-mounts .dxa files and loads
    pre-compressed .ktx2 textures directly to the GPU.

    ZIP entries use NoCompression since ETC2 is already compressed.

    Name fixups are applied after extraction to correct d2x-xl naming
    conventions that differ from the DXX engine's PIGfile bitmap names.
    See Rename-D2xxlTextures for the full list of fixups.

    Requires: 7-Zip (7z.exe), etc2tool (from android/tools/etc2tool/)
    Optional: ImageMagick (for high-quality downscaling)

.PARAMETER Game
    Which game to convert: "d1", "d2", or "both" (default: "both")

.PARAMETER MaxSize
    Maximum texture dimension. Textures larger than this are downscaled.
    Default: 0 (no limit). Useful values: 512, 1024

.PARAMETER TexSize
    Source archive texture size: 256 or 512. Default: 0 (use 512x512).
    Selects between D{1,2}-textures-256x256.7z and D{1,2}-textures-512x512.7z

.PARAMETER OutputDir
    Output directory for .dxa files. Default: same directory as this script

.PARAMETER SevenZip
    Path to 7z.exe. Default: "C:\Program Files\7-Zip\7z.exe"

.PARAMETER Etc2Tool
    Path to etc2tool.exe. Default: auto-detect from android/tools/etc2tool/build/Release/
#>
param(
    [ValidateSet("d1", "d2", "both")]
    [string]$Game = "both",

    [ValidateSet(0, 256, 512)]
    [int]$TexSize = 0,

    [int]$MaxSize = 0,

    [string]$OutputDir = "",

    [string]$SevenZip = "C:\Program Files\7-Zip\7z.exe",

    [string]$ReadmeText = "",

    [string]$ProjectReadmeText = "",

    [string]$Magick = "",

    [string]$Etc2Tool = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $OutputDir) { $OutputDir = $scriptDir }
$progressHelperScript = Join-Path $scriptDir "convert_progress_helpers.ps1"
if (-not (Test-Path $progressHelperScript)) {
    Write-Error "Missing helper script: $progressHelperScript"
    exit 1
}
. $progressHelperScript
$packLibScript = Join-Path $scriptDir "d2xxl_pack_lib.ps1"
if (-not (Test-Path $packLibScript)) {
    Write-Error "Missing helper script: $packLibScript"
    exit 1
}
. $packLibScript

# Auto-detect etc2tool if not provided
if (-not $Etc2Tool) {
    $repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path
    $candidate = Join-Path $repoRoot "android\tools\etc2tool\build\Release\etc2tool.exe"
    if (Test-Path $candidate) { $Etc2Tool = $candidate }
}
# TexSize 0 means use the 512x512 archives (original behavior)
$actualTexSize = if ($TexSize -eq 0) { 512 } else { $TexSize }
$archives = @{
    d1 = Join-Path $scriptDir "d2x-xl\D1-textures-${actualTexSize}x${actualTexSize}.7z"
    d2 = Join-Path $scriptDir "d2x-xl\D2-textures-${actualTexSize}x${actualTexSize}.7z"
}

# Read a TGA and return its Bitmap plus an optional supertransparency mask
$script:tgaMaxInputBytes = 268435456
$script:tgaMaxDimension = 8192
$script:tgaMaxPixels = 16777216

# D2X-XL uses the exact supertransparent key color RGB(120, 88, 128) in its
# TGA loader (`superTranspKeys` in gameio/tga.cpp) and mask creation path.
# Keep the converter exact as well so keyed overlays like door frames do not
# grow their masked region beyond what upstream treats as supertransparent.
$script:superTransparentKeyRed = 120
$script:superTransparentKeyGreen = 88
$script:superTransparentKeyBlue = 128

function Test-IsSuperTransparentColor {
    param(
        [byte]$Blue,
        [byte]$Green,
        [byte]$Red
    )

    return $Blue -eq $script:superTransparentKeyBlue -and
    $Green -eq $script:superTransparentKeyGreen -and
    $Red -eq $script:superTransparentKeyRed
}

function Assert-D2xxlTgaMetadataSpan {
    param(
        [long]$Offset,
        [long]$Length,
        [long]$ImageEnd,
        [long]$FooterOffset,
        [string]$Description
    )

    if ($Offset -lt $ImageEnd -or $Length -lt 0 -or $Offset -gt $FooterOffset -or $Length -gt ($FooterOffset - $Offset)) {
        throw "$Description is outside the TGA metadata area"
    }
}

function Get-D2xxlTgaLayout {
    param(
        [byte[]]$Bytes,
        [string]$Path
    )

    if ($Bytes.Length -lt 18) {
        throw "TGA header is truncated: $Path"
    }

    $identSize = [int]$Bytes[0]
    $colorMapType = [int]$Bytes[1]
    $imageType = [int]$Bytes[2]
    $cmapStart = [BitConverter]::ToUInt16($Bytes, 3)
    $cmapLength = [BitConverter]::ToUInt16($Bytes, 5)
    $cmapBits = [int]$Bytes[7]
    $width = [int][BitConverter]::ToUInt16($Bytes, 12)
    $height = [int][BitConverter]::ToUInt16($Bytes, 14)
    $bpp = [int]$Bytes[16]
    $descriptor = [int]$Bytes[17]

    if ($imageType -ne 2) {
        throw "Unsupported TGA type $imageType (only uncompressed true-color type 2 is supported): $Path"
    }
    if ($colorMapType -ne 0 -or $cmapStart -ne 0 -or $cmapLength -ne 0 -or $cmapBits -ne 0) {
        throw "Type-2 TGA must not declare a color map: $Path"
    }
    if ($width -le 0 -or $height -le 0 -or $width -gt $script:tgaMaxDimension -or $height -gt $script:tgaMaxDimension) {
        throw "TGA dimensions ${width}x${height} are outside the supported range: $Path"
    }
    $pixelCount = [long]$width * [long]$height
    if ($pixelCount -gt $script:tgaMaxPixels) {
        throw "TGA pixel count $pixelCount exceeds the supported limit: $Path"
    }
    if ($bpp -ne 24 -and $bpp -ne 32) {
        throw "Unsupported TGA bpp $bpp (only 24 or 32 is supported): $Path"
    }
    if (($descriptor -band 0xC0) -ne 0) {
        throw "Interleaved TGA storage is not supported: $Path"
    }
    $alphaBits = $descriptor -band 0x0F
    if (($bpp -eq 24 -and $alphaBits -ne 0) -or ($bpp -eq 32 -and $alphaBits -ne 0 -and $alphaBits -ne 8)) {
        throw "TGA descriptor alpha bits $alphaBits contradict $bpp-bpp pixels: $Path"
    }

    $channels = [int]($bpp / 8)
    $dataOffset = [long]18 + $identSize
    $pixelBytes = $pixelCount * $channels
    $imageEnd = $dataOffset + $pixelBytes
    if ($imageEnd -gt $Bytes.Length) {
        throw "TGA pixel payload is truncated: $Path"
    }

    if ($imageEnd -ne $Bytes.Length) {
        if ($Bytes.Length - $imageEnd -lt 26) {
            throw "TGA has trailing bytes without a complete 2.0 footer: $Path"
        }
        $footerOffset = [long]$Bytes.Length - 26
        $actualSignature = [BitConverter]::ToString($Bytes, [int]$footerOffset + 8, 18)
        $expectedSignature = [BitConverter]::ToString(
            [System.Text.Encoding]::ASCII.GetBytes("TRUEVISION-XFILE." + [char]0))
        if ($actualSignature -ne $expectedSignature) {
            throw "TGA trailing data does not end in a valid 2.0 footer: $Path"
        }
        if ($footerOffset -lt $imageEnd) {
            throw "TGA 2.0 footer overlaps pixel data: $Path"
        }

        $extensionOffset = [long][BitConverter]::ToUInt32($Bytes, [int]$footerOffset)
        $developerOffset = [long][BitConverter]::ToUInt32($Bytes, [int]$footerOffset + 4)
        if ($footerOffset -gt $imageEnd -and $extensionOffset -eq 0 -and $developerOffset -eq 0) {
            throw "TGA 2.0 footer does not describe its metadata bytes: $Path"
        }
        if ($extensionOffset -ne 0) {
            Assert-D2xxlTgaMetadataSpan -Offset $extensionOffset -Length 2 -ImageEnd $imageEnd -FooterOffset $footerOffset -Description "TGA extension header"
            $extensionSize = [long][BitConverter]::ToUInt16($Bytes, [int]$extensionOffset)
            if ($extensionSize -ne 495) {
                throw "TGA extension area has unsupported size $($extensionSize): $Path"
            }
            Assert-D2xxlTgaMetadataSpan -Offset $extensionOffset -Length $extensionSize -ImageEnd $imageEnd -FooterOffset $footerOffset -Description "TGA extension area"
        }
        if ($developerOffset -ne 0) {
            Assert-D2xxlTgaMetadataSpan -Offset $developerOffset -Length 2 -ImageEnd $imageEnd -FooterOffset $footerOffset -Description "TGA developer directory header"
            $tagCount = [long][BitConverter]::ToUInt16($Bytes, [int]$developerOffset)
            $directorySize = 2 + ($tagCount * 10)
            Assert-D2xxlTgaMetadataSpan -Offset $developerOffset -Length $directorySize -ImageEnd $imageEnd -FooterOffset $footerOffset -Description "TGA developer directory"
            for ($tag = 0; $tag -lt $tagCount; $tag++) {
                $entryOffset = [int]($developerOffset + 2 + ($tag * 10))
                $tagDataOffset = [long][BitConverter]::ToUInt32($Bytes, $entryOffset + 2)
                $tagDataSize = [long][BitConverter]::ToUInt32($Bytes, $entryOffset + 6)
                if ($tagDataSize -gt 0) {
                    Assert-D2xxlTgaMetadataSpan -Offset $tagDataOffset -Length $tagDataSize -ImageEnd $imageEnd -FooterOffset $footerOffset -Description "TGA developer tag $tag"
                }
            }
        }
    }

    return [pscustomobject]@{
        Width = $width
        Height = $height
        Channels = $channels
        AlphaBits = $alphaBits
        DataOffset = [int]$dataOffset
        TopOrigin = ($descriptor -band 0x20) -ne 0
        RightOrigin = ($descriptor -band 0x10) -ne 0
    }
}

function ConvertTo-D2xxlTgaBitmap {
    param(
        [int]$Width,
        [int]$Height,
        [byte[]]$Pixels,
        [System.Drawing.Imaging.PixelFormat]$PixelFormat,
        [int]$Channels
    )

    $bitmap = $null
    $bitmapData = $null
    $copied = $false
    $unlocked = $false
    try {
        $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, $PixelFormat)
        $bitmapData = $bitmap.LockBits(
            [System.Drawing.Rectangle]::new(0, 0, $Width, $Height),
            [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
            $PixelFormat)
        for ($row = 0; $row -lt $Height; $row++) {
            [System.Runtime.InteropServices.Marshal]::Copy(
                $Pixels,
                $row * $Width * $Channels,
                [IntPtr]::Add($bitmapData.Scan0, $row * $bitmapData.Stride),
                $Width * $Channels)
        }
        $copied = $true
    } finally {
        try {
            if ($bitmapData) {
                $bitmap.UnlockBits($bitmapData)
                $unlocked = $true
            }
        } finally {
            if ($bitmap -and (-not $copied -or -not $unlocked)) {
                $bitmap.Dispose()
            }
        }
    }
    return $bitmap
}

function Read-TGA {
    param([string]$Path)

    $fileLength = (Get-Item -LiteralPath $Path).Length
    if ($fileLength -gt $script:tgaMaxInputBytes) {
        throw "TGA exceeds the $($script:tgaMaxInputBytes)-byte input limit: $Path"
    }
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $layout = Get-D2xxlTgaLayout -Bytes $bytes -Path $Path
    $width = $layout.Width
    $height = $layout.Height
    $channels = $layout.Channels
    $pixelCount = $width * $height

    $hasKeyColor = $false
    for ($pixel = 0; $pixel -lt $pixelCount; $pixel++) {
        $idx = $layout.DataOffset + ($pixel * $channels)
        if (Test-IsSuperTransparentColor -Blue $bytes[$idx] -Green $bytes[$idx + 1] -Red $bytes[$idx + 2]) {
            $hasKeyColor = $true
            break
        }
    }

    $useArgb = ($channels -eq 4) -or $hasKeyColor
    $outputChannels = if ($useArgb) { 4 } else { 3 }
    $pixelFmt = if ($useArgb) {
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    } else {
        [System.Drawing.Imaging.PixelFormat]::Format24bppRgb
    }
    $outputBytes = [byte[]]::new($pixelCount * $outputChannels)
    [byte[]]$maskBytes = $null
    if ($hasKeyColor) {
        $maskBytes = [byte[]]::new($pixelCount * 4)
        for ($index = 0; $index -lt $maskBytes.Length; $index++) {
            $maskBytes[$index] = 255
        }
    }

    $hasTransparentPixels = $false
    $partialAlphaPixelCount = 0
    for ($storedY = 0; $storedY -lt $height; $storedY++) {
        $destY = if ($layout.TopOrigin) { $storedY } else { $height - 1 - $storedY }
        for ($storedX = 0; $storedX -lt $width; $storedX++) {
            $destX = if ($layout.RightOrigin) { $width - 1 - $storedX } else { $storedX }
            $source = $layout.DataOffset + (($storedY * $width + $storedX) * $channels)
            $destinationPixel = $destY * $width + $destX
            $destination = $destinationPixel * $outputChannels
            $isKey = Test-IsSuperTransparentColor -Blue $bytes[$source] -Green $bytes[$source + 1] -Red $bytes[$source + 2]
            if ($isKey) {
                $hasTransparentPixels = $true
                if ($maskBytes) {
                    $mask = $destinationPixel * 4
                    $maskBytes[$mask] = 0
                    $maskBytes[$mask + 1] = 0
                    $maskBytes[$mask + 2] = 0
                    $maskBytes[$mask + 3] = 0
                }
                continue
            }

            $outputBytes[$destination] = $bytes[$source]
            $outputBytes[$destination + 1] = $bytes[$source + 1]
            $outputBytes[$destination + 2] = $bytes[$source + 2]
            if ($useArgb) {
                $alpha = if ($channels -eq 4 -and $layout.AlphaBits -eq 8) { $bytes[$source + 3] } else { 255 }
                $outputBytes[$destination + 3] = $alpha
                if ($alpha -eq 0) {
                    $outputBytes[$destination] = 0
                    $outputBytes[$destination + 1] = 0
                    $outputBytes[$destination + 2] = 0
                    $hasTransparentPixels = $true
                } elseif ($alpha -lt 255) {
                    $partialAlphaPixelCount++
                }
            }
        }
    }

    $binaryCutoutPartialAlphaLimit = [Math]::Max(64, [int][Math]::Ceiling($pixelCount / 200.0))
    $hasBinaryCutout = $hasTransparentPixels -and ($partialAlphaPixelCount -le $binaryCutoutPartialAlphaLimit)

    # Edge color flood-fill prevents block compression from averaging black
    # against opaque colors at transparency boundaries
    if ($hasBinaryCutout -and $useArgb) {
        for ($y = 0; $y -lt $height; $y++) {
            for ($x = 0; $x -lt $width; $x++) {
                $pixel = ($y * $width + $x) * 4
                if ($outputBytes[$pixel + 3] -ne 0) {
                    continue
                }
                $sumBlue = 0
                $sumGreen = 0
                $sumRed = 0
                $neighborCount = 0
                for ($deltaY = -1; $deltaY -le 1; $deltaY++) {
                    for ($deltaX = -1; $deltaX -le 1; $deltaX++) {
                        if ($deltaY -eq 0 -and $deltaX -eq 0) {
                            continue
                        }
                        $neighborX = $x + $deltaX
                        $neighborY = $y + $deltaY
                        if ($neighborX -lt 0 -or $neighborX -ge $width -or $neighborY -lt 0 -or $neighborY -ge $height) {
                            continue
                        }
                        $neighbor = ($neighborY * $width + $neighborX) * 4
                        if ($outputBytes[$neighbor + 3] -eq 0) {
                            continue
                        }
                        $sumBlue += $outputBytes[$neighbor]
                        $sumGreen += $outputBytes[$neighbor + 1]
                        $sumRed += $outputBytes[$neighbor + 2]
                        $neighborCount++
                    }
                }
                if ($neighborCount -gt 0) {
                    $outputBytes[$pixel] = [byte]([Math]::Round($sumBlue / $neighborCount))
                    $outputBytes[$pixel + 1] = [byte]([Math]::Round($sumGreen / $neighborCount))
                    $outputBytes[$pixel + 2] = [byte]([Math]::Round($sumRed / $neighborCount))
                }
            }
        }
    }

    $bitmap = $null
    $maskBitmap = $null
    try {
        $bitmap = ConvertTo-D2xxlTgaBitmap -Width $width -Height $height -Pixels $outputBytes -PixelFormat $pixelFmt -Channels $outputChannels
        if ($maskBytes) {
            $maskBitmap = ConvertTo-D2xxlTgaBitmap `
                -Width $width `
                -Height $height `
                -Pixels $maskBytes `
                -PixelFormat ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb) `
                -Channels 4
        }
        return [pscustomobject]@{ Bitmap = $bitmap; Mask = $maskBitmap }
    } catch {
        if ($bitmap) {
            $bitmap.Dispose()
        }
        if ($maskBitmap) {
            $maskBitmap.Dispose()
        }
        throw
    }
}

# Split animation strip TGAs into individual frame files.
# d2x-xl stores animation frames as vertical strips: name#0.tga = WxNW (N frames).
# The engine expects separate files: name#0, name#1, ..., name#(N-1).
# Uses Read-TGA for correct alpha preservation and key color handling,
# then saves each frame as PNG. Replaces strip TGAs in-place.
function Split-StripTextures {
    param([string]$TgaDir, [string]$MagickPath)

    $splitCount = 0
    $tgaFiles = Get-ChildItem -Path $TgaDir -Filter "*.tga" -File
    foreach ($tga in $tgaFiles) {
        $bn = [IO.Path]::GetFileNameWithoutExtension($tga.Name)
        # Only process files that are frame-0 of an animation (name#0)
        if ($bn -notmatch '#0(-\w+)?$') { continue }

        $tgaResult = Read-TGA $tga.FullName
        $bmp = $tgaResult.Bitmap
        $maskBmp = $tgaResult.Mask
        try {
            $w = $bmp.Width
            $h = $bmp.Height
            if ($h -le $w -or $h % $w -ne 0) {
                continue
            }

            $nFrames = $h / $w
            if ($nFrames -le 1) {
                continue
            }

            # Extract the base name without the '#0' suffix and optional variant
            $variant = ""
            if ($bn -match '^(.+)#0(-\w+)$') {
                $nameBase = $Matches[1]
                $variant = $Matches[2]
            } else {
                $nameBase = $bn -replace '#0$', ''
            }

            Remove-Item $tga.FullName -Force

            # Read-TGA already handled alpha and key color
            for ($i = 0; $i -lt $nFrames; $i++) {
                $frameName = "${nameBase}#${i}${variant}.png"
                $framePath = Join-Path $TgaDir $frameName
                $y = $i * $w
                $rect = [System.Drawing.Rectangle]::new(0, $y, $w, $w)
                $frame = $null
                try {
                    $frame = $bmp.Clone($rect, $bmp.PixelFormat)
                    $frame.Save($framePath, [System.Drawing.Imaging.ImageFormat]::Png)
                } finally {
                    if ($frame) {
                        $frame.Dispose()
                    }
                }
                if ($maskBmp) {
                    $maskFrameName = "${nameBase}#${i}${variant}_mask.png"
                    $maskFramePath = Join-Path $TgaDir $maskFrameName
                    $maskFrame = $null
                    try {
                        $maskFrame = $maskBmp.Clone($rect, $maskBmp.PixelFormat)
                        $maskFrame.Save($maskFramePath, [System.Drawing.Imaging.ImageFormat]::Png)
                    } finally {
                        if ($maskFrame) {
                            $maskFrame.Dispose()
                        }
                    }
                }
            }
        } finally {
            $bmp.Dispose()
            if ($maskBmp) {
                $maskBmp.Dispose()
            }
        }
        $splitCount++
    }
    return $splitCount
}

function Convert-WithMagick {
    param(
        [string]$InputPath,
        [string]$OutputPath,
        [int]$MaxDim,
        [string]$MagickPath
    )
    $ErrorActionPreference = 'Continue'
    # Copy to safe temp name if '#' present (IM interprets '#N' as scene selector)
    $safeInput = $InputPath
    if ($InputPath -match '#') {
        $dir = Split-Path $InputPath
        $ext = [IO.Path]::GetExtension($InputPath)
        $safeInput = Join-Path $dir ("_safe_input_" + [IO.Path]::GetFileNameWithoutExtension($InputPath).Replace('#', '_H_') + $ext)
        Copy-Item $InputPath $safeInput -Force
    }
    $safeOutput = $OutputPath
    if ($OutputPath -match '#') {
        $dir = Split-Path $OutputPath
        $ext = [IO.Path]::GetExtension($OutputPath)
        $safeOutput = Join-Path $dir ("_safe_output_" + [IO.Path]::GetFileNameWithoutExtension($OutputPath).Replace('#', '_H_') + $ext)
    }
    # ImageMagick pipeline: sRGB -> linear -> Lanczos resize -> micro-sharpen -> sRGB
    $magickArgs = @(
        $safeInput,
        '-colorspace', 'RGB',
        '-filter', 'Lanczos',
        '-resize', "${MaxDim}x${MaxDim}",
        '-unsharp', '0x0.4',
        '-colorspace', 'sRGB'
    )
    if ($OutputPath -match '\.jpg$') {
        $magickArgs += @('-quality', '92')
    }
    $magickArgs += $safeOutput
    & $MagickPath $magickArgs 2>$null
    if ($safeInput -ne $InputPath) { Remove-Item $safeInput -Force -ErrorAction SilentlyContinue }
    if ($safeOutput -ne $OutputPath -and (Test-Path $safeOutput)) {
        Move-Item $safeOutput $OutputPath -Force
    }
    if ($LASTEXITCODE -ne 0) {
        throw "ImageMagick conversion failed for $InputPath"
    }
}

# ── d2x-xl -> DXX-Rebirth name fixups ──
#
# The d2x-xl texture packs use naming conventions that differ from the
# DXX engine's PIGfile bitmap names. The engine looks up hi-res textures
# by calling piggy_game_bitmap_name(bm) and appending ".ktx2", so the
# KTX2 entry names must match the PIGfile names exactly.
#
# NOTE: D1 has no naming convention mismatches. All D1 unmatched entries
# are addon/expansion content not in the base PIGfile (high-numbered
# rocks/doors/misc, CTF textures, lava variants, etc.). The fixups below
# are D2-specific but harmlessly no-op on D1 archives.
#
# Categories of mismatches and their resolution:
#
# 1. BOSS ANIMATION FRAMES (512px archive only, D2)
#    d2x-xl:  boss2_01.tga through boss2_15.tga  (underscore, 1-indexed)
#    PIGfile: boss02#0     through boss02#14      (hash, 0-indexed)
#    Fix: rename boss2_NN -> boss02#(NN-1)
#
# 2. ARROW EXCLAMATION SYNTAX (256px archive only, D2)
#    d2x-xl:  arw01!0.tga  ('!' used as alternate frame separator)
#    PIGfile: arw01#0      (engine only uses '#' for frame indexing)
#    Fix: rename replacing '!' with '#'
#    Note: arw01.tga (no frame suffix) also exists -- removed since the
#    engine only references the individual frames arw01#0 through arw01#5
#
# 3. TARGETING RETICLE COLOR VARIANTS (both archives, D2)
#    d2x-xl:  targ01b#0-green.tga, targ01b#0-red.tga, targ01b#0.tga
#    PIGfile: targ01b#0  (color is applied via PlayerCfg.ReticleRGBA tinting)
#    Fix: rename -green variants to the base name (overwriting the base).
#    The green set matches the original game. -red variants are left as-is
#    (unmatched, harmless in archive)
#
# 4. LAVA ZERO-PADDING (256px archive only, D2)
#    d2x-xl:  lava6#0.tga
#    PIGfile: lava06#0
#    Note: the 512 archive has lava06 (correct). 256 has lava6 (wrong).
#    Fix: not applied here -- the merge pass copies from 512 to 256. If
#    the 256 pack is rebuilt alone, add: lava6 -> lava06
#
# 5. D1-ONLY TEXTURE NAMES IN D2 PACK (512px archive, D2)
#    d2x-xl D2 pack includes: rock001, rock002, rock006, rock007, rock265,
#    metl131, metl132, metl133, metl134, metl135
#    These are Descent 1 PIGfile names. D2's PIGfile does not contain these
#    names (it uses different numbers for equivalent textures).
#    No fix applied -- left in archive harmlessly (no matching bitmap to bind to).
#    They ARE valid in the D1 pack
#
# 6. NON-PIGFILE ADDON TEXTURES (256px archive, D2)
#    blast, bubble, bullcase, bulletcase, corona, deadzone, fire, flare,
#    glare, halfhalo, halo, joymouse, rboticon, shield, smoke, sparks,
#    thrust2d, thrust3d
#    Plus from 512: cockpitbx2, statusbx2, hires-cockpit, monsterball,
#    mballmask, pwupicon
#    These are d2x-xl addon bitmaps not registered in the PIGfile.
#    piggy_game_bitmap_name() returns NULL for them, so KTX2 lookup is
#    skipped entirely. Fixing this would require game code changes to add
#    KTX2 lookup in each addon loading path.
#    Fix: none currently -- left in pack harmlessly (tiny disk overhead)
#    TODO: add engine-side KTX2 lookup for addon bitmaps if desired
#
# 7. BASE NAMES WITHOUT FRAME SUFFIX
#    d2x-xl:  arw01.tga, door01.tga (no #N frame suffix)
#    PIGfile: arw01#0, door01#0 (always has frame suffix for animated textures)
#    Rename-D2xxlTextures converts '!' to '#' first, then
#    Split-StripTextures splits into arw01#0, arw01#1, etc.
#    The leftover base file (if not a strip) doesn't match any PIGfile entry.
#    No fix applied -- left in archive harmlessly (tiny disk overhead)
#
function Rename-D2xxlTextures {
    param(
        [string]$TgaDir,
        [string]$GameId
    )

    $renamed = 0

    # --- Boss animation frames: boss2_NN -> boss02#(NN-1) ---
    foreach ($f in (Get-ChildItem -Path $TgaDir -Filter "boss2_*.tga" -File -ErrorAction SilentlyContinue)) {
        $bn = [IO.Path]::GetFileNameWithoutExtension($f.Name)
        if ($bn -match '^boss2_(\d+)$') {
            $frameIdx = [int]$Matches[1] - 1
            $newName = "boss02#${frameIdx}.tga"
            $newPath = Join-Path $TgaDir $newName
            if (-not (Test-Path $newPath)) {
                Rename-Item $f.FullName $newName
                $renamed++
            }
            # If target already exists (from a strip split), leave original as-is
        }
    }

    # --- Exclamation mark frame syntax: name!N -> name#N ---
    foreach ($f in (Get-ChildItem -Path $TgaDir -Filter "*!*.tga" -File -ErrorAction SilentlyContinue)) {
        $newName = $f.Name -replace '!', '#'
        $newPath = Join-Path $TgaDir $newName
        if (-not (Test-Path $newPath)) {
            Rename-Item $f.FullName $newName
            $renamed++
        }
        # If target already exists, leave original as-is
    }

    # --- Targeting reticle: rename -green variants to base name ---
    # Green set has better visual quality; overwrite the base file.
    # Red variants are left as-is (unmatched, harmless in archive).
    foreach ($f in (Get-ChildItem -Path $TgaDir -Filter "targ*-green.tga" -File -ErrorAction SilentlyContinue)) {
        $newName = $f.Name -replace '-green\.tga$', '.tga'
        $newPath = Join-Path $TgaDir $newName
        if (Test-Path $newPath) { Remove-Item $newPath -Force }
        Rename-Item $f.FullName $newName
        $renamed++
    }

    # D1-only names (rock001 etc) in D2 pack: left as-is (no matching D2 bitmap)
    # Base names without frame suffix (arw01.tga etc): left as-is after strip split
    # Non-PIGfile addon textures (blast, corona etc): left as-is
    # See doc comments above for full list of unmatched-but-harmless entries

    return @{ Renamed = $renamed }
}

function Convert-GameTextures {
    param(
        [string]$GameId,  # "d1" or "d2"
        [string]$ArchivePath,
        [string]$OutDir,
        [int]$MaxDim,
        [string]$Readme,
        [string]$ProjectReadme,
        [string]$MagickPath,
        [string]$Etc2ToolPath
    )

    if (-not (Test-Path $ArchivePath)) {
        Write-Host "Archive not found: $ArchivePath -- skipping $GameId"
        return
    }

    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "dxx_tex_convert_$GameId"
    if (Test-Path $tempDir) { Remove-Item -Recurse -Force $tempDir }
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    $sizeSuffix = if ($MaxDim -gt 0 -and $MaxDim -lt $actualTexSize) { $MaxDim } else { $actualTexSize }
    $dxaName = "${GameId}-hires-${sizeSuffix}-textures-ktx2.dxa"
    $dxaPath = Join-Path $OutDir $dxaName
    $totalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    $useMagick = $MagickPath -and $MaxDim -gt 0 -and (Test-Path $MagickPath)

    Write-Host "=== Converting $GameId textures ==="
    Write-Host "  Archive: $ArchivePath"
    Write-Host "  Output:  $dxaPath"
    if ($useMagick) {
        Write-Host "  Downscale: ImageMagick Lanczos (linear-light)"
    } elseif ($MaxDim -gt 0) {
        Write-Host "  Downscale: System.Drawing (fallback)"
    }

    # Extract TGA files from 7z
    $extractDir = Join-Path $tempDir "extract"
    Invoke-7ZipExtract -SevenZipPath $SevenZip -ArchivePath $ArchivePath -ExtractDir $extractDir

    $tgaDir = Join-Path (Join-Path $extractDir "textures") $GameId
    if (-not (Test-Path $tgaDir)) {
        Write-Host "  No textures/$GameId directory found in archive -- skipping"
        Remove-Item -Recurse -Force $tempDir
        return
    }

    # Rename d2x-xl files to DXX PIGfile names BEFORE splitting, because
    # some archives use '!' instead of '#' for frame separators (e.g. arw01!0)
    $fixupStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "  Scanning texture name fixups"
    $fixups = Rename-D2xxlTextures -TgaDir $tgaDir -GameId $GameId
    Write-Host "  Name fixup scan complete: $($fixups.Renamed) renamed in $(Format-ElapsedText $fixupStopwatch.Elapsed)"

    # Pre-split animation strips into individual frame files
    $splitStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "  Scanning animation strips"
    $splitCount = Split-StripTextures -TgaDir $tgaDir -MagickPath $MagickPath
    Write-Host "  Animation strip scan complete: $splitCount split in $(Format-ElapsedText $splitStopwatch.Elapsed)"

    # Collect all texture files: TGA (single frames), PNG (split frames), JPG
    $inventoryStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $tgaFiles = Get-ChildItem -Path $tgaDir -Filter "*.tga" -File
    $pngFiles = Get-ChildItem -Path $tgaDir -Filter "*.png" -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notlike '*_mask.png' }
    $jpgFiles = Get-ChildItem -Path $tgaDir -Filter "*.jpg" -File -ErrorAction SilentlyContinue
    $total = $tgaFiles.Count + ($pngFiles | Measure-Object).Count + ($jpgFiles | Measure-Object).Count
    Write-Host "  Inventory complete: TGA=$($tgaFiles.Count) PNG=$($pngFiles.Count) JPG=$($jpgFiles.Count) total=$total in $(Format-ElapsedText $inventoryStopwatch.Elapsed)"

    # Create ZIP (dxa)
    if (Test-Path $dxaPath) { Remove-Item $dxaPath }
    Write-Host "  Creating DXA container: $dxaName"
    $zip = [System.IO.Compression.ZipFile]::Open($dxaPath, [System.IO.Compression.ZipArchiveMode]::Create)
    $conversionStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    $converted = 0
    $skipped = 0
    $errors = 0
    $processed = 0

    # Cockpit overlays retain source quality up to the engine's hard safety cap
    $engineTextureCap = 2048
    $noDownscalePattern = '^(cockpit|hires-cockpit)'

    foreach ($tga in @($tgaFiles) + @($pngFiles)) {
        $processed++
        Write-ItemStartLine -Stopwatch $conversionStopwatch -Index $processed -Total $total -ItemName $tga.Name
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($tga.Name)
        $isTga = $tga.Extension -eq '.tga'

        # The quality exemption never bypasses the engine's safety cap
        $skipDownscale = $baseName -match $noDownscalePattern
        $effectiveMaxDim = if ($skipDownscale -or $MaxDim -le 0) { $engineTextureCap } else { [Math]::Min($MaxDim, $engineTextureCap) }
        $effectiveUseMagick = $MagickPath -and $effectiveMaxDim -gt 0 -and (Test-Path $MagickPath)

        try {
            $entryName = Get-D2xxlTextureEntryPath -GameId $GameId -BaseName $baseName
            $tempPng = Join-Path $tempDir "$baseName.png"
            $tempEtc2 = Join-Path $tempDir "$baseName.ktx2"

            if ($isTga) {
                # Read-TGA handles alpha preservation and key color
                # correctly by reading raw TGA bytes (not ImageMagick).
                # Also returns a mask for exact-key supertransparent textures
                $tgaResult = Read-TGA $tga.FullName
                $bmp = $tgaResult.Bitmap
                $maskBmp = $tgaResult.Mask
                $prePng = Join-Path $tempDir "$baseName.pre.png"
                $maskPrePng = $null
                try {
                    $bmp.Save($prePng, [System.Drawing.Imaging.ImageFormat]::Png)
                    $inputForResize = $prePng
                    if ($maskBmp) {
                        $maskPrePng = Join-Path $tempDir "${baseName}_mask.pre.png"
                        $maskBmp.Save($maskPrePng, [System.Drawing.Imaging.ImageFormat]::Png)
                    }
                } finally {
                    $bmp.Dispose()
                    if ($maskBmp) {
                        $maskBmp.Dispose()
                    }
                }
            } else {
                # PNG from strip splitter: already has correct alpha
                $inputForResize = $tga.FullName
                # Check for mask from strip splitter
                $splitMaskPath = Join-Path (Split-Path $tga.FullName) "${baseName}_mask.png"
                $maskPrePng = if (Test-Path $splitMaskPath) { $splitMaskPath } else { $null }
            }

            if ($effectiveUseMagick -and $effectiveMaxDim -gt 0) {
                # ImageMagick path: high-quality linear-light Lanczos downscale
                Convert-WithMagick -InputPath $inputForResize -OutputPath $tempPng -MaxDim $effectiveMaxDim -MagickPath $MagickPath
            } elseif ($isTga) {
                # Non-ImageMagick path: pre-PNG already created, just resize
                if ($effectiveMaxDim -gt 0) {
                    $bmp = [System.Drawing.Bitmap]::new($inputForResize)
                    if ($bmp.Width -gt $effectiveMaxDim -or $bmp.Height -gt $effectiveMaxDim) {
                        $scale = [Math]::Min($effectiveMaxDim / $bmp.Width, $effectiveMaxDim / $bmp.Height)
                        $newW = [Math]::Max(1, [int]($bmp.Width * $scale))
                        $newH = [Math]::Max(1, [int]($bmp.Height * $scale))
                        $resized = [System.Drawing.Bitmap]::new($bmp, $newW, $newH)
                        $bmp.Dispose()
                        $bmp = $resized
                    }
                    $bmp.Save($tempPng, [System.Drawing.Imaging.ImageFormat]::Png)
                    $bmp.Dispose()
                } else {
                    Copy-Item $inputForResize $tempPng -Force
                }
            } else {
                # PNG input, no ImageMagick, no resize: just copy
                Copy-Item $inputForResize $tempPng -Force
            }

            # Clean up pre-processed intermediate
            if ($isTga -and (Test-Path $inputForResize)) {
                Remove-Item $inputForResize -Force -ErrorAction SilentlyContinue
            }

            # Resize mask alongside main texture if present
            # Use nearest-neighbor (Point) filter for masks to keep binary
            # edges crisp -- Lanczos would anti-alias the mask boundaries
            $tempMaskPng = $null
            if ($maskPrePng -and (Test-Path $maskPrePng)) {
                $tempMaskPng = Join-Path $tempDir "${baseName}_mask.png"
                if ($effectiveUseMagick -and $effectiveMaxDim -gt 0) {
                    $safeMaskIn = $maskPrePng
                    if ($maskPrePng -match '#') {
                        $dir = Split-Path $maskPrePng
                        $ext = [IO.Path]::GetExtension($maskPrePng)
                        $safeMaskIn = Join-Path $dir ("_safe_mask_" + [IO.Path]::GetFileNameWithoutExtension($maskPrePng).Replace('#', '_H_') + $ext)
                        Copy-Item $maskPrePng $safeMaskIn -Force
                    }
                    $safeMaskOut = $tempMaskPng
                    if ($tempMaskPng -match '#') {
                        $dir = Split-Path $tempMaskPng
                        $ext = [IO.Path]::GetExtension($tempMaskPng)
                        $safeMaskOut = Join-Path $dir ("_safe_mask_out_" + [IO.Path]::GetFileNameWithoutExtension($tempMaskPng).Replace('#', '_H_') + $ext)
                    }
                    & $MagickPath $safeMaskIn -filter Point -resize "${effectiveMaxDim}x${effectiveMaxDim}" $safeMaskOut 2>$null
                    if ($safeMaskIn -ne $maskPrePng) { Remove-Item $safeMaskIn -Force -ErrorAction SilentlyContinue }
                    if ($safeMaskOut -ne $tempMaskPng -and (Test-Path $safeMaskOut)) {
                        Move-Item $safeMaskOut $tempMaskPng -Force
                    }
                } else {
                    Copy-Item $maskPrePng $tempMaskPng -Force
                }
                Remove-Item $maskPrePng -Force -ErrorAction SilentlyContinue
            }

            # Run etc2tool to compress to ETC2
            & $Etc2ToolPath $tempPng $tempEtc2 2>$null
            if ($LASTEXITCODE -ne 0) { throw "etc2tool failed for $baseName" }

            $entry = $zip.CreateEntry($entryName, [System.IO.Compression.CompressionLevel]::NoCompression)
            $stream = $entry.Open()
            $fileBytes = [System.IO.File]::ReadAllBytes($tempEtc2)
            $stream.Write($fileBytes, 0, $fileBytes.Length)
            $stream.Close()

            # Add super-transparent mask if present
            if ($tempMaskPng -and (Test-Path $tempMaskPng)) {
                $maskEntryName = Get-D2xxlMaskEntryPath -GameId $GameId -BaseName $baseName
                $maskEntry = $zip.CreateEntry($maskEntryName, [System.IO.Compression.CompressionLevel]::Optimal)
                $maskStream = $maskEntry.Open()
                $maskFileBytes = [System.IO.File]::ReadAllBytes($tempMaskPng)
                $maskStream.Write($maskFileBytes, 0, $maskFileBytes.Length)
                $maskStream.Close()
                Remove-Item $tempMaskPng -Force -ErrorAction SilentlyContinue
            }

            Remove-Item $tempPng, $tempEtc2 -ErrorAction SilentlyContinue

            $converted++
            if ($processed -le 3 -or $processed % 10 -eq 0 -or $processed -eq $total) {
                Write-ProgressSummaryLine -Stopwatch $conversionStopwatch -Processed $processed -Total $total -ItemName $tga.Name -Succeeded $converted -Errors $errors
            }
        } catch {
            Write-Host "  ERROR converting $($tga.Name): $_"
            $errors++
            Write-ProgressSummaryLine -Stopwatch $conversionStopwatch -Processed $processed -Total $total -ItemName $tga.Name -Succeeded $converted -Errors $errors
        }
    }

    # Handle JPG files (convert through etc2tool too)
    foreach ($jpg in $jpgFiles) {
        $processed++
        Write-ItemStartLine -Stopwatch $conversionStopwatch -Index $processed -Total $total -ItemName $jpg.Name
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($jpg.Name)
        $entryName = Get-D2xxlTextureEntryPath -GameId $GameId -BaseName $baseName
        $tempEtc2 = Join-Path $tempDir "$baseName.ktx2"

        # The quality exemption never bypasses the engine's safety cap
        $skipDownscale = $baseName -match $noDownscalePattern
        $effectiveMaxDim = if ($skipDownscale -or $MaxDim -le 0) { $engineTextureCap } else { [Math]::Min($MaxDim, $engineTextureCap) }
        $effectiveUseMagick = $MagickPath -and $effectiveMaxDim -gt 0 -and (Test-Path $MagickPath)

        try {
            if ($effectiveUseMagick -and $effectiveMaxDim -gt 0) {
                $tempPng = Join-Path $tempDir "$baseName.png"
                Convert-WithMagick -InputPath $jpg.FullName -OutputPath $tempPng -MaxDim $effectiveMaxDim -MagickPath $MagickPath
                & $Etc2ToolPath $tempPng $tempEtc2 2>$null
                Remove-Item $tempPng -ErrorAction SilentlyContinue
            } else {
                # etc2tool reads JPG directly via stb_image
                & $Etc2ToolPath $jpg.FullName $tempEtc2 2>$null
            }
            if ($LASTEXITCODE -ne 0) { throw "etc2tool failed for $baseName" }

            $entry = $zip.CreateEntry($entryName, [System.IO.Compression.CompressionLevel]::NoCompression)
            $stream = $entry.Open()
            $fileBytes = [System.IO.File]::ReadAllBytes($tempEtc2)
            $stream.Write($fileBytes, 0, $fileBytes.Length)
            $stream.Close()
            Remove-Item $tempEtc2 -ErrorAction SilentlyContinue
            $converted++
            if ($processed -le 3 -or $processed % 10 -eq 0 -or $processed -eq $total) {
                Write-ProgressSummaryLine -Stopwatch $conversionStopwatch -Processed $processed -Total $total -ItemName $jpg.Name -Succeeded $converted -Errors $errors
            }
        } catch {
            Write-Host "  ERROR converting $($jpg.Name): $_"
            $errors++
            Write-ProgressSummaryLine -Stopwatch $conversionStopwatch -Processed $processed -Total $total -ItemName $jpg.Name -Succeeded $converted -Errors $errors
        }
    }

    # Add README.md if provided
    if ($Readme) {
        $readmeEntry = $zip.CreateEntry("README.md", [System.IO.Compression.CompressionLevel]::Optimal)
        $readmeStream = $readmeEntry.Open()
        $readmeBytes = [System.Text.Encoding]::UTF8.GetBytes($Readme)
        $readmeStream.Write($readmeBytes, 0, $readmeBytes.Length)
        $readmeStream.Close()
    }
    if ($ProjectReadme) {
        $projectReadmeEntry = $zip.CreateEntry((Get-D2xxlProjectReadmeEntryName), [System.IO.Compression.CompressionLevel]::Optimal)
        $projectReadmeStream = $projectReadmeEntry.Open()
        $projectReadmeBytes = [System.Text.Encoding]::UTF8.GetBytes($ProjectReadme)
        $projectReadmeStream.Write($projectReadmeBytes, 0, $projectReadmeBytes.Length)
        $projectReadmeStream.Close()
    }

    $finalizeStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "  Finalizing archive: $dxaName"
    $zip.Dispose()
    Write-Host "  Archive finalized in $(Format-ElapsedText $finalizeStopwatch.Elapsed)"
    Remove-Item -Recurse -Force $tempDir

    if ($errors -gt 0) {
        Remove-Item -LiteralPath $dxaPath -Force -ErrorAction SilentlyContinue
        throw "$errors texture conversion errors prevented publication of $dxaName"
    }

    $dxaSize = (Get-Item $dxaPath).Length / 1MB
    Write-Host "  Done: processed $processed / $total, converted $converted, errors $errors in $(Format-ElapsedText $totalStopwatch.Elapsed)"
    Write-Host "  Output: $dxaPath ($([Math]::Round($dxaSize, 1)) MB)"
}

# ── Main ──

if ($MyInvocation.InvocationName -eq '.') {
    return
}

if (-not $Etc2Tool -or -not (Test-Path $Etc2Tool)) {
    Write-Error "etc2tool not found. Build it first: cd android/tools/etc2tool && cmake -B build && cmake --build build --config Release"
    exit 1
}

if (-not (Test-Path $SevenZip)) {
    Write-Error "7-Zip not found at: $SevenZip"
    exit 1
}

$games = if ($Game -eq "both") { @("d1", "d2") } else { @($Game) }

foreach ($g in $games) {
    Convert-GameTextures -GameId $g -ArchivePath $archives[$g] -OutDir $OutputDir -MaxDim $MaxSize -Readme $ReadmeText -ProjectReadme $ProjectReadmeText -MagickPath $Magick -Etc2ToolPath $Etc2Tool
}

Write-Host ""
Write-Host "Conversion complete. Place .dxa files in the game data directory"
Write-Host "to enable hires textures (pre-compressed ETC2 format)"
