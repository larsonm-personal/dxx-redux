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

    Requires: 7-Zip (7z.exe), etc2tool (from tools/etc2tool/)
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
    Path to etc2tool.exe. Default: auto-detect from tools/etc2tool/build/Release/
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
    $candidate = Join-Path $repoRoot "tools\etc2tool\build\Release\etc2tool.exe"
    if (Test-Path $candidate) { $Etc2Tool = $candidate }
}
if (-not $Etc2Tool -or -not (Test-Path $Etc2Tool)) {
    Write-Error "etc2tool not found. Build it first: cd tools/etc2tool && cmake -B build && cmake --build build --config Release"
    exit 1
}

# TexSize 0 means use the 512x512 archives (original behavior)
$actualTexSize = if ($TexSize -eq 0) { 512 } else { $TexSize }
$archives = @{
    d1 = Join-Path $scriptDir "d2x-xl\D1-textures-${actualTexSize}x${actualTexSize}.7z"
    d2 = Join-Path $scriptDir "d2x-xl\D2-textures-${actualTexSize}x${actualTexSize}.7z"
}

# Read a TGA and return a Bitmap with correct alpha handling.
# Sets $script:lastTransparencyMask to a mask Bitmap (alpha=0 where the
# texture should fully punch through, alpha=255 elsewhere) for exact
# D2X-XL supertransparent key-color textures, or $null otherwise.
$script:lastTransparencyMask = $null

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

function Read-TGA {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 18) { throw "TGA too small: $Path" }

    $identSize = $bytes[0]
    $colorMapType = $bytes[1]
    $imageType = $bytes[2]
    # Color map spec: bytes 3-7
    $cmapStart = [BitConverter]::ToUInt16($bytes, 3)
    $cmapLength = [BitConverter]::ToUInt16($bytes, 5)
    $cmapBits = $bytes[7]
    # Image spec: bytes 8-17
    $xOrigin = [BitConverter]::ToUInt16($bytes, 8)
    $yOrigin = [BitConverter]::ToUInt16($bytes, 10)
    $width = [BitConverter]::ToUInt16($bytes, 12)
    $height = [BitConverter]::ToUInt16($bytes, 14)
    $bpp = $bytes[16]
    $descriptor = $bytes[17]

    if ($imageType -ne 2) {
        throw "Unsupported TGA type $imageType (only uncompressed RGB/RGBA supported): $Path"
    }
    if ($bpp -ne 24 -and $bpp -ne 32) {
        throw "Unsupported TGA bpp $bpp (only 24 or 32 supported): $Path"
    }

    $channels = $bpp / 8
    $topToBottom = ($descriptor -band 0x20) -ne 0

    # Skip ID field and color map
    $dataOffset = 18 + $identSize
    if ($colorMapType -eq 1) {
        $dataOffset += $cmapLength * [Math]::Ceiling($cmapBits / 8)
    }

    $hasKeyColor = $false
    if ($channels -eq 3) {
        for ($idx = $dataOffset; $idx -le $bytes.Length - 3; $idx += 3) {
            if (Test-IsSuperTransparentColor -Blue $bytes[$idx] -Green $bytes[$idx + 1] -Red $bytes[$idx + 2]) {
                $hasKeyColor = $true
                break
            }
        }
    }

    $useArgb = ($channels -eq 4) -or $hasKeyColor
    $pixelFmt = if ($useArgb) {
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    } else {
        [System.Drawing.Imaging.PixelFormat]::Format24bppRgb
    }
    $outputChannels = if ($useArgb) { 4 } else { 3 }

    $bmp = [System.Drawing.Bitmap]::new($width, $height, $pixelFmt)
    $bmpData = $bmp.LockBits(
        [System.Drawing.Rectangle]::new(0, 0, $width, $height),
        [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
        $pixelFmt
    )

    $stride = $bmpData.Stride
    $scan0 = $bmpData.Scan0
    $rowBytes = $width * $channels
    $copyRowBytes = $width * $outputChannels
    $alphaBits = if ($channels -eq 4) { $descriptor -band 0x0F } else { 0 }

    # Track binary cutout information for super-transparent mask generation
    $hasTransparentPixels = $false
    $partialAlphaPixelCount = 0
    $maskBytes = $null
    if ($useArgb) {
        # Pre-allocate mask: 4 bytes/pixel (BGRA), all white/opaque
        $maskBytes = [byte[]]::new($height * $width * 4)
        for ($mi = 0; $mi -lt $maskBytes.Length; $mi += 4) {
            $maskBytes[$mi] = 255; $maskBytes[$mi + 1] = 255
            $maskBytes[$mi + 2] = 255; $maskBytes[$mi + 3] = 255
        }
    }

    for ($row = 0; $row -lt $height; $row++) {
        # TGA default is bottom-to-top unless top-to-bottom flag is set
        $srcRow = if ($topToBottom) { $row } else { $height - 1 - $row }
        $srcOffset = $dataOffset + $srcRow * $rowBytes
        if ($channels -eq 4) {
            for ($px = 0; $px -lt $rowBytes; $px += 4) {
                $idx = $srcOffset + $px
                # BGRA byte order: B=$bytes[idx], G=[idx+1], R=[idx+2], A=[idx+3]
                if (Test-IsSuperTransparentColor -Blue $bytes[$idx] -Green $bytes[$idx + 1] -Red $bytes[$idx + 2]) {
                    $bytes[$idx] = 0; $bytes[$idx + 1] = 0
                    $bytes[$idx + 2] = 0; $bytes[$idx + 3] = 0
                    $hasKeyColor = $true
                    $hasTransparentPixels = $true
                    $mIdx = ($row * $width + $px / 4) * 4
                    $maskBytes[$mIdx] = 0; $maskBytes[$mIdx + 1] = 0
                    $maskBytes[$mIdx + 2] = 0; $maskBytes[$mIdx + 3] = 0
                } elseif ($alphaBits -gt 0) {
                    if ($bytes[$idx + 3] -eq 0) {
                        $bytes[$idx] = 0; $bytes[$idx + 1] = 0; $bytes[$idx + 2] = 0
                        $hasTransparentPixels = $true
                    } elseif ($bytes[$idx + 3] -lt 255) {
                        $partialAlphaPixelCount++
                    }
                } else {
                    $bytes[$idx + 3] = 255
                }
            }
            # TGA stores BGR(A), System.Drawing also uses BGR(A), so direct copy works
            [System.Runtime.InteropServices.Marshal]::Copy(
                $bytes, $srcOffset, [IntPtr]::Add($scan0, $row * $stride), $copyRowBytes
            )
        } elseif ($useArgb) {
            $rowBuffer = [byte[]]::new($copyRowBytes)
            for ($px = 0; $px -lt $rowBytes; $px += 3) {
                $idx = $srcOffset + $px
                $dst = ($px / 3) * 4
                if (Test-IsSuperTransparentColor -Blue $bytes[$idx] -Green $bytes[$idx + 1] -Red $bytes[$idx + 2]) {
                    $hasTransparentPixels = $true
                    $mIdx = ($row * $width + $px / 3) * 4
                    $maskBytes[$mIdx] = 0; $maskBytes[$mIdx + 1] = 0
                    $maskBytes[$mIdx + 2] = 0; $maskBytes[$mIdx + 3] = 0
                    $rowBuffer[$dst] = 0; $rowBuffer[$dst + 1] = 0
                    $rowBuffer[$dst + 2] = 0; $rowBuffer[$dst + 3] = 0
                } else {
                    $rowBuffer[$dst] = $bytes[$idx]
                    $rowBuffer[$dst + 1] = $bytes[$idx + 1]
                    $rowBuffer[$dst + 2] = $bytes[$idx + 2]
                    $rowBuffer[$dst + 3] = 255
                }
            }
            [System.Runtime.InteropServices.Marshal]::Copy(
                $rowBuffer, 0, [IntPtr]::Add($scan0, $row * $stride), $copyRowBytes
            )
        } else {
            [System.Runtime.InteropServices.Marshal]::Copy(
                $bytes, $srcOffset, [IntPtr]::Add($scan0, $row * $stride), $rowBytes
            )
        }
    }

    $bmp.UnlockBits($bmpData)

    $binaryCutoutPartialAlphaLimit = [Math]::Max(64, [int][Math]::Ceiling(($width * $height) / 200.0))
    $hasBinaryCutout = $hasTransparentPixels -and ($partialAlphaPixelCount -le $binaryCutoutPartialAlphaLimit)

    # Edge color flood-fill: replace RGB of fully transparent pixels with
    # the average RGB of their non-transparent neighbors. This prevents
    # ETC2 block compression from averaging black against opaque colors
    # at transparency boundaries, which causes visible color fringing.
    if ($hasBinaryCutout -and $useArgb) {
        $bmpData2 = $bmp.LockBits(
            [System.Drawing.Rectangle]::new(0, 0, $width, $height),
            [System.Drawing.Imaging.ImageLockMode]::ReadWrite,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $stride2 = $bmpData2.Stride
        $pixBuf = [byte[]]::new($height * $stride2)
        [System.Runtime.InteropServices.Marshal]::Copy($bmpData2.Scan0, $pixBuf, 0, $pixBuf.Length)

        for ($y = 0; $y -lt $height; $y++) {
            for ($x = 0; $x -lt $width; $x++) {
                $pi = $y * $stride2 + $x * 4
                if ($pixBuf[$pi + 3] -ne 0) { continue }  # skip opaque
                # Average RGB from non-transparent neighbors
                $sumB = 0; $sumG = 0; $sumR = 0; $cnt = 0
                for ($dy = -1; $dy -le 1; $dy++) {
                    for ($dx = -1; $dx -le 1; $dx++) {
                        if ($dy -eq 0 -and $dx -eq 0) { continue }
                        $nx = $x + $dx; $ny = $y + $dy
                        if ($nx -lt 0 -or $nx -ge $width -or $ny -lt 0 -or $ny -ge $height) { continue }
                        $ni = $ny * $stride2 + $nx * 4
                        if ($pixBuf[$ni + 3] -eq 0) { continue }
                        $sumB += $pixBuf[$ni]; $sumG += $pixBuf[$ni + 1]; $sumR += $pixBuf[$ni + 2]
                        $cnt++
                    }
                }
                if ($cnt -gt 0) {
                    $pixBuf[$pi] = [byte]([Math]::Round($sumB / $cnt))
                    $pixBuf[$pi + 1] = [byte]([Math]::Round($sumG / $cnt))
                    $pixBuf[$pi + 2] = [byte]([Math]::Round($sumR / $cnt))
                    # alpha stays 0
                }
            }
        }
        [System.Runtime.InteropServices.Marshal]::Copy($pixBuf, 0, $bmpData2.Scan0, $pixBuf.Length)
        $bmp.UnlockBits($bmpData2)
    }

    # Generate mask bitmap only for exact key-color textures. Plain alpha-cutout
    # replacements like metl154 still benefit from the edge-bleed fix above,
    # but they should keep ordinary alpha semantics instead of being promoted
    # into supertransparency.
    $script:lastTransparencyMask = $null
    if ($hasKeyColor -and $maskBytes) {
        $maskBmp = [System.Drawing.Bitmap]::new($width, $height,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $maskData = $maskBmp.LockBits(
            [System.Drawing.Rectangle]::new(0, 0, $width, $height),
            [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $maskStride = $maskData.Stride
        for ($row = 0; $row -lt $height; $row++) {
            [System.Runtime.InteropServices.Marshal]::Copy(
                $maskBytes, $row * $width * 4,
                [IntPtr]::Add($maskData.Scan0, $row * $maskStride),
                $width * 4)
        }
        $maskBmp.UnlockBits($maskData)
        $script:lastTransparencyMask = $maskBmp
    }

    return $bmp
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

        $bmp = Read-TGA $tga.FullName
        $maskBmp = $script:lastTransparencyMask
        $w = $bmp.Width; $h = $bmp.Height
        if ($h -le $w -or $h % $w -ne 0) { $bmp.Dispose(); if ($maskBmp) { $maskBmp.Dispose() }; continue }

        $nFrames = $h / $w
        if ($nFrames -le 1) { $bmp.Dispose(); if ($maskBmp) { $maskBmp.Dispose() }; continue }

        # Extract the base name without the '#0' suffix (and optional variant like '-green')
        $variant = ""
        if ($bn -match '^(.+)#0(-\w+)$') {
            $nameBase = $Matches[1]; $variant = $Matches[2]
        } else {
            $nameBase = $bn -replace '#0$', ''
        }

        # Remove original strip TGA
        Remove-Item $tga.FullName -Force

        # Crop each frame via Bitmap.Clone and save as PNG.
        # Read-TGA already handled alpha + key color, so the PNG
        # has correct transparency data.
        for ($i = 0; $i -lt $nFrames; $i++) {
            $frameName = "${nameBase}#${i}${variant}.png"
            $framePath = Join-Path $TgaDir $frameName
            $y = $i * $w
            $rect = [System.Drawing.Rectangle]::new(0, $y, $w, $w)
            $frame = $bmp.Clone($rect, $bmp.PixelFormat)
            $frame.Save($framePath, [System.Drawing.Imaging.ImageFormat]::Png)
            $frame.Dispose()
            # Split mask frame if mask exists
            if ($maskBmp) {
                $maskFrameName = "${nameBase}#${i}${variant}_mask.png"
                $maskFramePath = Join-Path $TgaDir $maskFrameName
                $maskFrame = $maskBmp.Clone($rect, $maskBmp.PixelFormat)
                $maskFrame.Save($maskFramePath, [System.Drawing.Imaging.ImageFormat]::Png)
                $maskFrame.Dispose()
            }
        }

        $bmp.Dispose()
        if ($maskBmp) { $maskBmp.Dispose() }
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

    # Textures that should never be downscaled: full-screen cockpit overlays
    # and targeting reticles are HUD elements drawn at screen resolution
    $noDownscalePattern = '^(cockpit|hires-cockpit)'

    foreach ($tga in @($tgaFiles) + @($pngFiles)) {
        $processed++
        Write-ItemStartLine -Stopwatch $conversionStopwatch -Index $processed -Total $total -ItemName $tga.Name
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($tga.Name)
        $isTga = $tga.Extension -eq '.tga'

        # Skip downscaling for HUD/cockpit textures
        $skipDownscale = $MaxDim -gt 0 -and $baseName -match $noDownscalePattern
        $effectiveMaxDim = if ($skipDownscale) { 0 } else { $MaxDim }
        $effectiveUseMagick = $MagickPath -and $effectiveMaxDim -gt 0 -and (Test-Path $MagickPath)

        try {
            $entryName = Get-D2xxlTextureEntryPath -GameId $GameId -BaseName $baseName
            $tempPng = Join-Path $tempDir "$baseName.png"
            $tempEtc2 = Join-Path $tempDir "$baseName.ktx2"

            if ($isTga) {
                # Read-TGA handles alpha preservation and key color
                # correctly by reading raw TGA bytes (not ImageMagick).
                # Also sets $script:lastTransparencyMask for exact-key
                # supertransparent textures.
                $bmp = Read-TGA $tga.FullName
                $maskBmp = $script:lastTransparencyMask
                $prePng = Join-Path $tempDir "$baseName.pre.png"
                $bmp.Save($prePng, [System.Drawing.Imaging.ImageFormat]::Png)
                $bmp.Dispose()
                $inputForResize = $prePng
                # Save mask pre-resize PNG if present
                $maskPrePng = $null
                if ($maskBmp) {
                    $maskPrePng = Join-Path $tempDir "${baseName}_mask.pre.png"
                    $maskBmp.Save($maskPrePng, [System.Drawing.Imaging.ImageFormat]::Png)
                    $maskBmp.Dispose()
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

        # Skip downscaling for HUD/cockpit textures
        $skipDownscale = $MaxDim -gt 0 -and $baseName -match $noDownscalePattern
        $effectiveMaxDim = if ($skipDownscale) { 0 } else { $MaxDim }
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

    $dxaSize = (Get-Item $dxaPath).Length / 1MB
    Write-Host "  Done: processed $processed / $total, converted $converted, errors $errors in $(Format-ElapsedText $totalStopwatch.Elapsed)"
    Write-Host "  Output: $dxaPath ($([Math]::Round($dxaSize, 1)) MB)"
}

# ── Main ──

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
