#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Converts all d2x-xl texture and sound packs into .dxa mod files.

.DESCRIPTION
    Calls convert_d2xxl_textures.ps1 for each combination of game (d1/d2)
    and texture size (128/256/512), then convert_d2xxl_sounds.ps1 for both
    games. Produces 6 texture .dxa files and 2 sound .dxa files.

    Output files:
      d1-hires-128-textures-ktx2.dxa
      d1-hires-256-textures-ktx2.dxa
      d1-hires-512-textures-ktx2.dxa
      d2-hires-128-textures-ktx2.dxa
      d2-hires-256-textures-ktx2.dxa
      d2-hires-512-textures-ktx2.dxa
      d1-hires-sounds.dxa
      d2-hires-sounds.dxa

.PARAMETER OutputDir
    Output directory for .dxa files. Default: same directory as this script

.PARAMETER SevenZip
    Path to 7z.exe. Default: "C:\Program Files\7-Zip\7z.exe"
#>
param(
    [string]$OutputDir = "",
    [string]$SevenZip = "C:\Program Files\7-Zip\7z.exe",
    [string]$Magick = "",
    [string]$Etc2Tool = ""
)

$ErrorActionPreference = "Stop"
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
$repoRoot = (Resolve-Path (Join-Path $scriptDir "..\..\..")).Path

# Auto-locate etc2tool
if (-not $Etc2Tool) {
    $candidate = Join-Path $repoRoot "android\tools\etc2tool\build\Release\etc2tool.exe"
    if (Test-Path $candidate) { $Etc2Tool = $candidate }
}
if (-not $Etc2Tool -or -not (Test-Path $Etc2Tool)) {
    Write-Error "etc2tool not found. Build it first: cd android/tools/etc2tool && cmake -B build && cmake --build build --config Release"
    exit 1
}
Write-Host "etc2tool: $Etc2Tool"

$texScript = Join-Path $scriptDir "convert_d2xxl_textures.ps1"
$sndScript = Join-Path $scriptDir "convert_d2xxl_sounds.ps1"

# Auto-locate ImageMagick if not provided
if (-not $Magick) {
    $depBaseFile = Join-Path $repoRoot "dependency_base.txt"
    if (Test-Path $depBaseFile) {
        $depBase = (Get-Content $depBaseFile -First 1).Trim()
        $magickDir = Get-ChildItem -Path $depBase -Directory -Filter "imagemagick-*" -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending | Select-Object -First 1
        if ($magickDir) {
            $candidate = Join-Path $magickDir.FullName "magick.exe"
            if (Test-Path $candidate) { $Magick = $candidate }
        }
    }
    if (-not $Magick) {
        $onPath = Get-Command magick -ErrorAction SilentlyContinue
        if ($onPath) { $Magick = $onPath.Source }
    }
}
if ($Magick) {
    Write-Host "ImageMagick: $Magick"
} else {
    Write-Host "WARNING: ImageMagick not found; 128px packs will use System.Drawing fallback"
}

$projectReadme = Get-D2xxlProjectReadmeText -ScriptDir $scriptDir

# Texture size configs: (TexSize=source archive, MaxSize=downscale target, Downscaled flag)
$texConfigs = @(
    @{ TexSize = 512; MaxSize = 128; Downscaled = $true },
    @{ TexSize = 256; MaxSize = 0; Downscaled = $false },
    @{ TexSize = 512; MaxSize = 0; Downscaled = $false }
)

# ── Merge pass: equalize file counts across all three sizes ──
# The 256 and 512 source archives contain different texture sets.
# After the initial build, each pack only has textures from its source.
# This pass fills gaps: textures only in 256 get added to 512+128,
# textures only in 512 get downscaled and added to 256.

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-DxaEtc2Names {
    param([string]$DxaPath)
    if (-not (Test-Path $DxaPath)) { return @() }
    $zip = [System.IO.Compression.ZipFile]::OpenRead($DxaPath)
    $names = $zip.Entries | Where-Object { $_.Name -like '*.ktx2' } |
        ForEach-Object { [System.IO.Path]::GetFileNameWithoutExtension($_.Name) }
    $zip.Dispose()
    return @($names)
}

function Add-Etc2ToDxa {
    param(
        [string]$GameId,
        [string]$DxaPath,
        [string[]]$Etc2Files,
        [string]$Indent = "    "
    )

    $updateStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "${Indent}Updating $(Split-Path $DxaPath -Leaf) with $($Etc2Files.Count) files"
    # Keep the existing archive readable if any source read or ZIP update fails
    $candidate = $DxaPath + '.building-' + [guid]::NewGuid().ToString('N')
    try {
        Copy-Item -LiteralPath $DxaPath -Destination $candidate -Force
        $zip = [System.IO.Compression.ZipFile]::Open($candidate, [System.IO.Compression.ZipArchiveMode]::Update)
        try {
            $fileIndex = 0
            foreach ($f in $Etc2Files) {
                $fileIndex++
                $baseName = [System.IO.Path]::GetFileNameWithoutExtension($f)
                # Restore '#' from safe naming used during conversion
                $baseName = $baseName -replace '_H_', '#'
                $entryName = Get-D2xxlTextureEntryPath -GameId $GameId -BaseName $baseName
                if ($fileIndex -le 3 -or $fileIndex % 25 -eq 0 -or $fileIndex -eq $Etc2Files.Count) {
                    Write-Host "${Indent}[$((Format-ElapsedText $updateStopwatch.Elapsed))] Adding $fileIndex / $($Etc2Files.Count): $entryName"
                }
                if ($zip.GetEntry($entryName)) { throw "Merge cannot add an existing texture: $entryName" }
                $fileBytes = [System.IO.File]::ReadAllBytes($f)
                $entry = $zip.CreateEntry($entryName, [System.IO.Compression.CompressionLevel]::NoCompression)
                $entry.LastWriteTime = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
                $stream = $entry.Open()
                try {
                    $stream.Write($fileBytes, 0, $fileBytes.Length)
                } finally {
                    $stream.Dispose()
                }
                Add-D2xxlSpriteBlendEntry -Zip $zip -GameId $GameId -BaseName $baseName
                $maskPath = Join-Path ([IO.Path]::GetDirectoryName($f)) ([IO.Path]::GetFileNameWithoutExtension($f) + '_mask.png')
                if (Test-Path -LiteralPath $maskPath) {
                    $maskEntry = $zip.CreateEntry((Get-D2xxlMaskEntryPath -GameId $GameId -BaseName $baseName), [System.IO.Compression.CompressionLevel]::Optimal)
                    $maskEntry.LastWriteTime = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
                    $maskStream = $maskEntry.Open()
                    try {
                        $maskBytes = [System.IO.File]::ReadAllBytes($maskPath)
                        $maskStream.Write($maskBytes, 0, $maskBytes.Length)
                    } finally {
                        $maskStream.Dispose()
                    }
                }
            }
            $paths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            foreach ($existing in $zip.Entries) {
                if (!$paths.Add($existing.FullName)) { throw "Duplicate merge archive path: $($existing.FullName)" }
            }
        } finally {
            $zip.Dispose()
        }
        if ((Get-FileHash -LiteralPath $candidate).Hash -ne (Get-FileHash -LiteralPath $DxaPath).Hash) {
            Move-Item -LiteralPath $candidate -Destination $DxaPath -Force
        }
    } finally {
        if (Test-Path -LiteralPath $candidate) { Remove-Item -LiteralPath $candidate -Force }
    }
    Write-Host "${Indent}Archive update complete in $(Format-ElapsedText $updateStopwatch.Elapsed)"
}

function Convert-AndAdd {
    param(
        [string]$GameId,
        [string]$ArchivePath,     # source 7z to extract from
        [string[]]$MissingNames,  # texture basenames to process
        [hashtable[]]$Targets     # @{DxaPath, MaxDim} -- packs to add results to
    )

    $ErrorActionPreference = 'Stop'
    $totalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    $mergeRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'android/temp/d2xxl-merge'))
    $tempDir = Join-Path $mergeRoot ('run_' + [guid]::NewGuid().ToString('N'))
    & (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $tempDir -DirectoryPrefix run_
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    $extractDir = Join-Path $tempDir "extract"
    Invoke-7ZipExtract -SevenZipPath $SevenZip -ArchivePath $ArchivePath -ExtractDir $extractDir -Indent "  "
    $tgaDir = Join-Path (Join-Path $extractDir "textures") $GameId

    # Use the same TGA decoder, exact-key masks and strip splitter as primary conversion
    # ImageMagick reads several original TGA files as opaque despite their alpha bytes
    . $texScript -SevenZip $SevenZip -Magick $Magick -Etc2Tool $Etc2Tool
    Rename-D2xxlTextures -TgaDir $tgaDir -GameId $GameId | Out-Null
    Split-StripTextures -TgaDir $tgaDir -MagickPath $Magick -GameId $GameId | Out-Null

    # Textures that should never be downscaled: full-screen cockpit overlays
    $noDownscalePattern = '^(cockpit|hires-cockpit)'

    foreach ($tgt in $Targets) {
        $targetStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $maxDim = $tgt.MaxDim
        $dxaPath = $tgt.DxaPath
        $label = if ($maxDim -gt 0) { $maxDim } else { "native" }
        $outDir = Join-Path $tempDir "out_$label"
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null

        $converted = 0
        $errors = 0
        $processed = 0
        $mergeTotal = $MissingNames.Count
        $etc2Files = @()
        Write-Host "    Target ${label}: processing $mergeTotal textures"
        foreach ($name in $MissingNames) {
            $processed++
            Write-ItemStartLine -Stopwatch $targetStopwatch -Index $processed -Total $mergeTotal -ItemName $name -Indent "      "
            $src = Join-Path $tgaDir "$name.png"
            if (-not (Test-Path $src)) { $src = Join-Path $tgaDir "$name.tga" }
            if (-not (Test-Path $src)) { $src = Join-Path $tgaDir "$name.jpg" }
            if (-not (Test-Path $src)) {
                $errors++
                Write-Host "      Missing source for $name"
                Write-ProgressSummaryLine -Stopwatch $targetStopwatch -Processed $processed -Total $mergeTotal -ItemName $name -Succeeded $converted -Errors $errors -Indent "      "
                continue
            }

            try {
                $safeName = $name -replace '#', '_H_'
                $tempEtc2 = Join-Path $outDir "$safeName.ktx2"
                $normalized = Join-Path $outDir "${safeName}_source.png"
                $sourceMask = Join-Path $tgaDir "${name}_mask.png"
                if ([IO.Path]::GetExtension($src) -eq '.tga') {
                    $decoded = Read-TGA $src -D2xxlSource
                    try {
                        $decoded.Bitmap.Save($normalized, [System.Drawing.Imaging.ImageFormat]::Png)
                        if ($decoded.Mask) {
                            $sourceMask = Join-Path $outDir "${safeName}_source_mask.png"
                            $decoded.Mask.Save($sourceMask, [System.Drawing.Imaging.ImageFormat]::Png)
                        }
                    } finally {
                        $decoded.Bitmap.Dispose()
                        if ($decoded.Mask) { $decoded.Mask.Dispose() }
                    }
                } else {
                    # Use a safe filename so ImageMagick cannot interpret frame suffixes
                    $normalized = Join-Path $outDir ("${safeName}_source" + [IO.Path]::GetExtension($src))
                    Copy-Item -LiteralPath $src -Destination $normalized -Force
                }
                # Skip downscaling for HUD/cockpit textures
                $skipQualityDownscale = $name -match $noDownscalePattern
                $effectiveMaxDim = if ($skipQualityDownscale -or $maxDim -le 0) { 2048 } else { [Math]::Min($maxDim, 2048) }
                if ($effectiveMaxDim -gt 0 -and $Magick) {
                    # Copy to safe name to avoid IM interpreting '#' as scene selector
                    $tempPng = Join-Path $outDir "$safeName.png"
                    Convert-WithMagick -InputPath $normalized -OutputPath $tempPng -MaxDim $effectiveMaxDim -MagickPath $Magick
                    & $Etc2Tool $tempPng $tempEtc2 2>$null
                    Remove-Item $tempPng -ErrorAction SilentlyContinue
                } else {
                    & $Etc2Tool $normalized $tempEtc2 2>$null
                }
                if ($LASTEXITCODE -ne 0) { throw "etc2tool failed" }
                if (Test-Path -LiteralPath $sourceMask) {
                    $tempMask = Join-Path $outDir "${safeName}_mask.png"
                    $safeMaskSource = Join-Path $outDir "${safeName}_mask_input.png"
                    Copy-Item -LiteralPath $sourceMask -Destination $safeMaskSource -Force
                    if ($effectiveMaxDim -gt 0 -and $Magick) {
                        & $Magick $safeMaskSource -filter Point -resize "${effectiveMaxDim}x${effectiveMaxDim}>" -define 'png:exclude-chunk=date,time' $tempMask 2>$null
                        if ($LASTEXITCODE -ne 0) { throw "mask resize failed" }
                    } else {
                        Copy-Item -LiteralPath $safeMaskSource -Destination $tempMask -Force
                    }
                }
                $etc2Files += $tempEtc2
                $converted++
                if ($processed -le 3 -or $processed % 10 -eq 0 -or $processed -eq $mergeTotal) {
                    Write-ProgressSummaryLine -Stopwatch $targetStopwatch -Processed $processed -Total $mergeTotal -ItemName $name -Succeeded $converted -Errors $errors -Indent "      "
                }
            } catch {
                Write-Host "    ERROR: $name -- $_"
                $errors++
                Write-ProgressSummaryLine -Stopwatch $targetStopwatch -Processed $processed -Total $mergeTotal -ItemName $name -Succeeded $converted -Errors $errors -Indent "      "
            }
        }

        if ($errors -gt 0) { throw "$errors merge conversion errors for $GameId $label" }
        if ($etc2Files.Count -gt 0) {
            Write-Host "    Adding $converted textures to $(Split-Path $dxaPath -Leaf)"
            Add-Etc2ToDxa -GameId $GameId -DxaPath $dxaPath -Etc2Files $etc2Files -Indent "      "
        } else {
            Write-Host "    No converted textures for $(Split-Path $dxaPath -Leaf)"
        }

        Write-Host "    Target $label complete: processed $processed / $mergeTotal, converted $converted, errors $errors in $(Format-ElapsedText $targetStopwatch.Elapsed)"
    }

    $resolvedTemp = (Resolve-Path -LiteralPath $tempDir).Path
    if (![IO.Path]::GetFullPath($resolvedTemp).StartsWith($mergeRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Merge cleanup escaped its workspace: $resolvedTemp"
    }
    Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
    Write-Host "  Merge source $GameId complete in $(Format-ElapsedText $totalStopwatch.Elapsed)"
}

if ($MyInvocation.InvocationName -eq '.') { return }

Write-Host "=== All-in-one d2x-xl mod conversion ==="
Write-Host ""

foreach ($game in @("d1", "d2")) {
    foreach ($cfg in $texConfigs) {
        $sz = $cfg.TexSize
        $mx = $cfg.MaxSize
        $label = if ($mx -gt 0) { $mx } else { $sz }
        $packStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $readme = Get-D2xxlTextureReadmeText -GameId $game -Size $label -Downscaled $cfg.Downscaled
        # Always pass ImageMagick: strip splitting needs it even without downscaling
        $magickArg = if ($Magick) { $Magick } else { "" }
        Write-Host "--- Textures: $game ${label}x${label} ---"
        & $texScript -Game $game -TexSize $sz -MaxSize $mx -OutputDir $OutputDir -SevenZip $SevenZip -ReadmeText $readme -ProjectReadmeText $projectReadme -Magick $magickArg -Etc2Tool $Etc2Tool
        if ($LASTEXITCODE -ne 0) {
            throw "Texture conversion failed for $game $label"
        } else {
            Write-Host "--- Textures complete: $game ${label}x${label} in $(Format-ElapsedText $packStopwatch.Elapsed) ---"
        }
        Write-Host ""
    }
}

Write-Host "--- Merge pass: equalizing texture counts ---"
$archiveDirPath = Join-Path $scriptDir "d2x-xl"

foreach ($game in @("d1", "d2")) {
    $dxa128 = Join-Path $OutputDir "${game}-hires-128-textures-ktx2.dxa"
    $dxa256 = Join-Path $OutputDir "${game}-hires-256-textures-ktx2.dxa"
    $dxa512 = Join-Path $OutputDir "${game}-hires-512-textures-ktx2.dxa"
    $arch256 = Join-Path $archiveDirPath "$($game.ToUpper())-textures-256x256.7z"
    $arch512 = Join-Path $archiveDirPath "$($game.ToUpper())-textures-512x512.7z"

    $names256 = Get-DxaEtc2Names $dxa256
    $names512 = Get-DxaEtc2Names $dxa512

    # Textures in 256 source but not 512 -- add to 512 (at 256px) and 128 (downscaled)
    $onlyIn256 = @($names256 | Where-Object { $_ -notin $names512 })
    if ($onlyIn256.Count -gt 0) {
        Write-Host "  $($game.ToUpper()): $($onlyIn256.Count) textures only in 256 source"
        Convert-AndAdd -GameId $game -ArchivePath $arch256 -MissingNames $onlyIn256 -Targets @(
            @{ DxaPath = $dxa512; MaxDim = 0 },
            @{ DxaPath = $dxa128; MaxDim = 128 }
        )
    }

    # Textures in 512 source but not 256 -- downscale to 256 and add
    $onlyIn512 = @($names512 | Where-Object { $_ -notin $names256 })
    if ($onlyIn512.Count -gt 0) {
        Write-Host "  $($game.ToUpper()): $($onlyIn512.Count) textures only in 512 source"
        Convert-AndAdd -GameId $game -ArchivePath $arch512 -MissingNames $onlyIn512 -Targets @(
            @{ DxaPath = $dxa256; MaxDim = 256 }
        )
    }

    # Verify
    $f128 = (Get-DxaEtc2Names $dxa128).Count
    $f256 = (Get-DxaEtc2Names $dxa256).Count
    $f512 = (Get-DxaEtc2Names $dxa512).Count
    Write-Host "  $($game.ToUpper()) final: 128=$f128 256=$f256 512=$f512"
}
Write-Host ""

# Sounds: both games
Write-Host "--- Sounds: d1 + d2 ---"
foreach ($game in @("d1", "d2")) {
    $soundStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $soundReadme = Get-D2xxlSoundReadmeText -GameId $game
    & $sndScript -Game $game -OutputDir $OutputDir -SevenZip $SevenZip -ReadmeText $soundReadme -ProjectReadmeText $projectReadme
    if ($LASTEXITCODE -ne 0) {
        throw "Sound conversion failed for $game"
    } else {
        Write-Host "--- Sounds complete: $game in $(Format-ElapsedText $soundStopwatch.Elapsed) ---"
    }
}

Write-Host ""
Write-Host "=== All conversions complete ==="
Write-Host "Output directory: $OutputDir"
Get-ChildItem -Path $OutputDir -Filter "*-hires-*.dxa" | ForEach-Object {
    $sizeMB = [Math]::Round($_.Length / 1MB, 1)
    Write-Host "  $($_.Name) ($sizeMB MB)"
}
