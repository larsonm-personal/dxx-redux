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

    [string]$Magick = "",

    [string]$Etc2Tool = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $OutputDir) { $OutputDir = $scriptDir }

# Auto-detect etc2tool if not provided
if (-not $Etc2Tool) {
    $repoRoot = Split-Path (Split-Path $scriptDir)
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

function Read-TGA {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 18) { throw "TGA too small: $Path" }

    $identSize   = $bytes[0]
    $colorMapType = $bytes[1]
    $imageType   = $bytes[2]
    # Color map spec: bytes 3-7
    $cmapStart   = [BitConverter]::ToUInt16($bytes, 3)
    $cmapLength  = [BitConverter]::ToUInt16($bytes, 5)
    $cmapBits    = $bytes[7]
    # Image spec: bytes 8-17
    $xOrigin     = [BitConverter]::ToUInt16($bytes, 8)
    $yOrigin     = [BitConverter]::ToUInt16($bytes, 10)
    $width       = [BitConverter]::ToUInt16($bytes, 12)
    $height      = [BitConverter]::ToUInt16($bytes, 14)
    $bpp         = $bytes[16]
    $descriptor  = $bytes[17]

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

    $pixelFmt = if ($channels -eq 4) {
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    } else {
        [System.Drawing.Imaging.PixelFormat]::Format24bppRgb
    }

    $bmp = [System.Drawing.Bitmap]::new($width, $height, $pixelFmt)
    $bmpData = $bmp.LockBits(
        [System.Drawing.Rectangle]::new(0, 0, $width, $height),
        [System.Drawing.Imaging.ImageLockMode]::WriteOnly,
        $pixelFmt
    )

    $stride = $bmpData.Stride
    $scan0 = $bmpData.Scan0
    $rowBytes = $width * $channels

    for ($row = 0; $row -lt $height; $row++) {
        # TGA default is bottom-to-top unless top-to-bottom flag is set
        $srcRow = if ($topToBottom) { $row } else { $height - 1 - $row }
        $srcOffset = $dataOffset + $srcRow * $rowBytes
        # For 32bpp TGA, convert super-transparent key color RGB(120,88,128)
        # to RGBA(0,0,0,0). Uses per-channel tolerance of 13 (~5%) to
        # catch anti-aliased edges. Zeroing RGB prevents ETC2 color
        # bleeding at compressed block boundaries.
        # Alpha handling: if the TGA declares alphaBits > 0, the alpha
        # channel contains real per-pixel transparency data (used by
        # many d2x-xl textures instead of or alongside key color).
        # Preserve native alpha in that case. Only force alpha=255 for
        # non-key pixels when alphaBits == 0 (truly undefined alpha).
        if ($channels -eq 4) {
            $alphaBits = $descriptor -band 0x0F
            for ($px = 0; $px -lt $rowBytes; $px += 4) {
                $idx = $srcOffset + $px
                # BGRA byte order: B=$bytes[idx], G=[idx+1], R=[idx+2], A=[idx+3]
                $db = [int]$bytes[$idx] - 128
                $dg = [int]$bytes[$idx + 1] - 88
                $dr = [int]$bytes[$idx + 2] - 120
                if ($dr -ge -13 -and $dr -le 13 -and $dg -ge -13 -and $dg -le 13 -and $db -ge -13 -and $db -le 13) {
                    $bytes[$idx] = 0; $bytes[$idx + 1] = 0
                    $bytes[$idx + 2] = 0; $bytes[$idx + 3] = 0
                } elseif ($alphaBits -gt 0) {
                    # Native alpha: zero RGB for fully transparent pixels
                    if ($bytes[$idx + 3] -eq 0) {
                        $bytes[$idx] = 0; $bytes[$idx + 1] = 0; $bytes[$idx + 2] = 0
                    }
                } else {
                    $bytes[$idx + 3] = 255
                }
            }
        }
        # TGA stores BGR(A), System.Drawing also uses BGR(A), so direct copy works
        [System.Runtime.InteropServices.Marshal]::Copy(
            $bytes, $srcOffset, [IntPtr]::Add($scan0, $row * $stride), $rowBytes
        )
    }

    $bmp.UnlockBits($bmpData)
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
        $w = $bmp.Width; $h = $bmp.Height
        if ($h -le $w -or $h % $w -ne 0) { $bmp.Dispose(); continue }

        $nFrames = $h / $w
        if ($nFrames -le 1) { $bmp.Dispose(); continue }

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
        }

        $bmp.Dispose()
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
        $safeInput = Join-Path $dir ("_safe_input_" + [IO.Path]::GetFileNameWithoutExtension($InputPath).Replace('#','_H_') + $ext)
        Copy-Item $InputPath $safeInput -Force
    }
    $safeOutput = $OutputPath
    if ($OutputPath -match '#') {
        $dir = Split-Path $OutputPath
        $ext = [IO.Path]::GetExtension($OutputPath)
        $safeOutput = Join-Path $dir ("_safe_output_" + [IO.Path]::GetFileNameWithoutExtension($OutputPath).Replace('#','_H_') + $ext)
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
    Write-Host "  Extracting archive..."
    $extractDir = Join-Path $tempDir "extract"
    & $SevenZip x "-o$extractDir" $ArchivePath -y 2>$null
    if ($LASTEXITCODE -ne 0) { throw "7z extraction failed" }

    $tgaDir = Join-Path (Join-Path $extractDir "textures") $GameId
    if (-not (Test-Path $tgaDir)) {
        Write-Host "  No textures/$GameId directory found in archive -- skipping"
        Remove-Item -Recurse -Force $tempDir
        return
    }

    # Rename d2x-xl files to DXX PIGfile names BEFORE splitting, because
    # some archives use '!' instead of '#' for frame separators (e.g. arw01!0)
    $fixups = Rename-D2xxlTextures -TgaDir $tgaDir -GameId $GameId
    if ($fixups.Renamed -gt 0) {
        Write-Host "  Name fixups: $($fixups.Renamed) renamed"
    }

    # Pre-split animation strips into individual frame files
    $splitCount = Split-StripTextures -TgaDir $tgaDir -MagickPath $MagickPath
    if ($splitCount -gt 0) {
        Write-Host "  Split $splitCount animation strips into individual frames"
    }

    # Collect all texture files: TGA (single frames), PNG (split frames), JPG
    $tgaFiles = Get-ChildItem -Path $tgaDir -Filter "*.tga" -File
    $pngFiles = Get-ChildItem -Path $tgaDir -Filter "*.png" -File -ErrorAction SilentlyContinue
    $jpgFiles = Get-ChildItem -Path $tgaDir -Filter "*.jpg" -File -ErrorAction SilentlyContinue
    $total = $tgaFiles.Count + ($pngFiles | Measure-Object).Count + ($jpgFiles | Measure-Object).Count
    Write-Host "  Found $total texture files"

    # Create ZIP (dxa)
    if (Test-Path $dxaPath) { Remove-Item $dxaPath }
    $zip = [System.IO.Compression.ZipFile]::Open($dxaPath, [System.IO.Compression.ZipArchiveMode]::Create)

    $converted = 0
    $skipped = 0
    $errors = 0

    foreach ($tga in @($tgaFiles) + @($pngFiles)) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($tga.Name)
        $isTga = $tga.Extension -eq '.tga'

        try {
            $entryName = "$baseName.ktx2"
            $tempPng = Join-Path $tempDir "$baseName.png"
            $tempEtc2 = Join-Path $tempDir "$baseName.ktx2"

            if ($isTga) {
                # Read-TGA handles alpha preservation and key color
                # correctly by reading raw TGA bytes (not ImageMagick).
                $bmp = Read-TGA $tga.FullName
                $prePng = Join-Path $tempDir "$baseName.pre.png"
                $bmp.Save($prePng, [System.Drawing.Imaging.ImageFormat]::Png)
                $bmp.Dispose()
                $inputForResize = $prePng
            } else {
                # PNG from strip splitter: already has correct alpha
                $inputForResize = $tga.FullName
            }

            if ($useMagick -and $MaxDim -gt 0) {
                # ImageMagick path: high-quality linear-light Lanczos downscale
                Convert-WithMagick -InputPath $inputForResize -OutputPath $tempPng -MaxDim $MaxDim -MagickPath $MagickPath
            } elseif ($isTga) {
                # Non-ImageMagick path: pre-PNG already created, just resize
                if ($MaxDim -gt 0) {
                    $bmp = [System.Drawing.Bitmap]::new($inputForResize)
                    if ($bmp.Width -gt $MaxDim -or $bmp.Height -gt $MaxDim) {
                        $scale = [Math]::Min($MaxDim / $bmp.Width, $MaxDim / $bmp.Height)
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

            # Run etc2tool to compress to ETC2
            & $Etc2ToolPath $tempPng $tempEtc2 2>$null
            if ($LASTEXITCODE -ne 0) { throw "etc2tool failed for $baseName" }

            $entry = $zip.CreateEntry($entryName, [System.IO.Compression.CompressionLevel]::NoCompression)
            $stream = $entry.Open()
            $fileBytes = [System.IO.File]::ReadAllBytes($tempEtc2)
            $stream.Write($fileBytes, 0, $fileBytes.Length)
            $stream.Close()
            Remove-Item $tempPng, $tempEtc2 -ErrorAction SilentlyContinue

            $converted++
            if ($converted % 10 -eq 0) {
                Write-Host "  Converted $converted / $total..."
            }
        } catch {
            Write-Host "  ERROR converting $($tga.Name): $_"
            $errors++
        }
    }

    # Handle JPG files (convert through etc2tool too)
    foreach ($jpg in $jpgFiles) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($jpg.Name)
        $entryName = "$baseName.ktx2"
        $tempEtc2 = Join-Path $tempDir "$baseName.ktx2"

        try {
            if ($useMagick -and $MaxDim -gt 0) {
                $tempPng = Join-Path $tempDir "$baseName.png"
                Convert-WithMagick -InputPath $jpg.FullName -OutputPath $tempPng -MaxDim $MaxDim -MagickPath $MagickPath
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
        } catch {
            Write-Host "  ERROR converting $($jpg.Name): $_"
            $errors++
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

    $zip.Dispose()
    Remove-Item -Recurse -Force $tempDir

    $dxaSize = (Get-Item $dxaPath).Length / 1MB
    Write-Host "  Done: $converted converted, $errors errors"
    Write-Host "  Output: $dxaPath ($([Math]::Round($dxaSize, 1)) MB)"
}

# ── Main ──

if (-not (Test-Path $SevenZip)) {
    Write-Error "7-Zip not found at: $SevenZip"
    exit 1
}

$games = if ($Game -eq "both") { @("d1", "d2") } else { @($Game) }

foreach ($g in $games) {
    Convert-GameTextures -GameId $g -ArchivePath $archives[$g] -OutDir $OutputDir -MaxDim $MaxSize -Readme $ReadmeText -MagickPath $Magick -Etc2ToolPath $Etc2Tool
}

Write-Host ""
Write-Host "Conversion complete. Place .dxa files in the game data directory"
Write-Host "to enable hires textures (pre-compressed ETC2 format)"
