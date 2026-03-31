<#
.SYNOPSIS
    Converts d2x-xl high-res TGA texture packs into .dxa (ZIP) files
    loadable by the DXX-Rebirth/Redux engine.

.DESCRIPTION
    Extracts TGA files from the d2x-xl 7z archives, compresses them to
    ETC2 format via etc2tool, and packages the .etc2 files into .dxa
    files (ZIP archives). The engine auto-mounts .dxa files and loads
    pre-compressed .etc2 textures directly to the GPU.

    ZIP entries use NoCompression since ETC2 is already compressed.

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
# Replaces strip TGAs with individual frame TGAs in-place.
# Requires ImageMagick. Note: '#' in filenames is dangerous for IM -- uses safe temp names.
function Split-StripTextures {
    param([string]$TgaDir, [string]$MagickPath)
    if (-not $MagickPath -or -not (Test-Path $MagickPath)) { return 0 }

    $splitCount = 0
    $tgaFiles = Get-ChildItem -Path $TgaDir -Filter "*.tga" -File
    foreach ($tga in $tgaFiles) {
        $bn = [IO.Path]::GetFileNameWithoutExtension($tga.Name)
        # Only process files that are frame-0 of an animation (name#0)
        if ($bn -notmatch '#0(-\w+)?$') { continue }

        # Copy to safe name to avoid IM interpreting '#' as scene selector
        $safeName = $tga.Name -replace '#', '__H__'
        $safePath = Join-Path $TgaDir $safeName
        Copy-Item $tga.FullName $safePath -Force

        $dims = & $MagickPath identify -format "%w %h" $safePath 2>&1
        if ($LASTEXITCODE -ne 0) { Remove-Item $safePath -Force; continue }
        $parts = ("$dims" -split '\s+')
        $w = [int]$parts[0]; $h = [int]$parts[1]
        if ($h -le $w -or $h % $w -ne 0) { Remove-Item $safePath -Force; continue }

        $nFrames = $h / $w
        if ($nFrames -le 1) { Remove-Item $safePath -Force; continue }

        # Extract the base name without the '#0' suffix (and optional variant like '-green')
        $variant = ""
        if ($bn -match '^(.+)#0(-\w+)$') {
            $nameBase = $Matches[1]; $variant = $Matches[2]
        } else {
            $nameBase = $bn -replace '#0$', ''
        }

        # Crop each frame and save as individual TGA
        for ($i = 0; $i -lt $nFrames; $i++) {
            $frameName = "${nameBase}#${i}${variant}.tga"
            $safeFrameName = $frameName -replace '#', '__H__'
            $safeFramePath = Join-Path $TgaDir $safeFrameName
            $y = $i * $w
            & $MagickPath $safePath -crop "${w}x${w}+0+${y}" +repage $safeFramePath 2>&1 | Out-Null
            if ($LASTEXITCODE -eq 0) {
                # Rename safe frame to real name
                $realFramePath = Join-Path $TgaDir $frameName
                if (Test-Path $realFramePath) { Remove-Item $realFramePath -Force }
                Rename-Item $safeFramePath $frameName
            }
        }

        # Remove the original strip TGA and safe copy
        Remove-Item $tga.FullName -Force
        Remove-Item $safePath -Force -ErrorAction SilentlyContinue
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
    & $MagickPath $magickArgs 2>&1 | Out-Null
    if ($safeInput -ne $InputPath) { Remove-Item $safeInput -Force -ErrorAction SilentlyContinue }
    if ($safeOutput -ne $OutputPath -and (Test-Path $safeOutput)) {
        Move-Item $safeOutput $OutputPath -Force
    }
    if ($LASTEXITCODE -ne 0) {
        throw "ImageMagick conversion failed for $InputPath"
    }
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
    $dxaName = "d2xxl-hires-textures-${GameId}-${sizeSuffix}.dxa"
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
    & $SevenZip x "-o$extractDir" $ArchivePath -y 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "7z extraction failed" }

    $tgaDir = Join-Path (Join-Path $extractDir "textures") $GameId
    if (-not (Test-Path $tgaDir)) {
        Write-Host "  No textures/$GameId directory found in archive -- skipping"
        Remove-Item -Recurse -Force $tempDir
        return
    }

    # Pre-split animation strips into individual frame files
    $splitCount = Split-StripTextures -TgaDir $tgaDir -MagickPath $MagickPath
    if ($splitCount -gt 0) {
        Write-Host "  Split $splitCount animation strips into individual frames"
    }

    $tgaFiles = Get-ChildItem -Path $tgaDir -Filter "*.tga" -File
    $jpgFiles = Get-ChildItem -Path $tgaDir -Filter "*.jpg" -File -ErrorAction SilentlyContinue
    $total = $tgaFiles.Count + ($jpgFiles | Measure-Object).Count
    Write-Host "  Found $total texture files"

    # Create ZIP (dxa)
    if (Test-Path $dxaPath) { Remove-Item $dxaPath }
    $zip = [System.IO.Compression.ZipFile]::Open($dxaPath, [System.IO.Compression.ZipArchiveMode]::Create)

    $converted = 0
    $skipped = 0
    $errors = 0

    foreach ($tga in $tgaFiles) {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($tga.Name)

        try {
            $entryName = "$baseName.etc2"
            $tempPng = Join-Path $tempDir "$baseName.png"
            $tempEtc2 = Join-Path $tempDir "$baseName.etc2"

            if ($useMagick -and $MaxDim -gt 0) {
                # ImageMagick path: high-quality linear-light Lanczos downscale -> temp PNG
                Convert-WithMagick -InputPath $tga.FullName -OutputPath $tempPng -MaxDim $MaxDim -MagickPath $MagickPath
            } else {
                # System.Drawing path: TGA parse + optional resize -> temp PNG
                $bmp = Read-TGA $tga.FullName
                if ($MaxDim -gt 0 -and ($bmp.Width -gt $MaxDim -or $bmp.Height -gt $MaxDim)) {
                    $scale = [Math]::Min($MaxDim / $bmp.Width, $MaxDim / $bmp.Height)
                    $newW = [Math]::Max(1, [int]($bmp.Width * $scale))
                    $newH = [Math]::Max(1, [int]($bmp.Height * $scale))
                    $resized = [System.Drawing.Bitmap]::new($bmp, $newW, $newH)
                    $bmp.Dispose()
                    $bmp = $resized
                }
                $bmp.Save($tempPng, [System.Drawing.Imaging.ImageFormat]::Png)
                $bmp.Dispose()
            }

            # Run etc2tool to compress to ETC2
            & $Etc2ToolPath $tempPng $tempEtc2 2>&1 | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "etc2tool failed for $baseName" }

            $entry = $zip.CreateEntry($entryName, [System.IO.Compression.CompressionLevel]::NoCompression)
            $stream = $entry.Open()
            $fileBytes = [System.IO.File]::ReadAllBytes($tempEtc2)
            $stream.Write($fileBytes, 0, $fileBytes.Length)
            $stream.Close()
            Remove-Item $tempPng, $tempEtc2 -ErrorAction SilentlyContinue

            $converted++
            if ($converted % 50 -eq 0) {
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
        $entryName = "$baseName.etc2"
        $tempEtc2 = Join-Path $tempDir "$baseName.etc2"

        try {
            if ($useMagick -and $MaxDim -gt 0) {
                $tempPng = Join-Path $tempDir "$baseName.png"
                Convert-WithMagick -InputPath $jpg.FullName -OutputPath $tempPng -MaxDim $MaxDim -MagickPath $MagickPath
                & $Etc2ToolPath $tempPng $tempEtc2 2>&1 | Out-Null
                Remove-Item $tempPng -ErrorAction SilentlyContinue
            } else {
                # etc2tool reads JPG directly via stb_image
                & $Etc2ToolPath $jpg.FullName $tempEtc2 2>&1 | Out-Null
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
