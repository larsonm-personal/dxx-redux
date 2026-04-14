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
$repoRoot = Split-Path (Split-Path $scriptDir)

# Auto-locate etc2tool
if (-not $Etc2Tool) {
    $candidate = Join-Path $repoRoot "tools\etc2tool\build\Release\etc2tool.exe"
    if (Test-Path $candidate) { $Etc2Tool = $candidate }
}
if (-not $Etc2Tool -or -not (Test-Path $Etc2Tool)) {
    Write-Error "etc2tool not found. Build it first: cd tools/etc2tool && cmake -B build && cmake --build build --config Release"
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

# Credits
$creditsD2 = "Aus-RED-5D, DizzyRox, MetalBeast, Novacron, Theftbot"
$creditsD1 = "DizzyRox, Novacron, Aus-RED-5"

# Per-pack README content (multi-line, embedded in each .dxa)
function Get-ReadmeText {
    param([string]$GameId, [int]$Size, [bool]$Downscaled)
    $credits = if ($GameId -eq "d2") { $creditsD2 } else { $creditsD1 }
    $gameName = if ($GameId -eq "d2") { "Descent 2" } else { "Descent 1" }
    $packName = "${GameId}-hires-${Size}-textures-ktx2"
    $mobileNote = "Textures are ETC2-compressed in KTX2 containers for mobile/Android"
    if ($Downscaled) {
        @"
# ${packName}

${Size}x${Size} replacement textures for ${gameName}

Downscaled from the 512x512 d2x-xl texture pack using ImageMagick
with linear-light colorspace conversion, Lanczos resampling, and
micro-sharpening to preserve detail at reduced resolution

${mobileNote}

Original textures from d2x-xl by ${credits}
"@
    } else {
        @"
# ${packName}

${Size}x${Size} high-resolution replacement textures for ${gameName}

${mobileNote}

Original textures from d2x-xl by ${credits}
"@
    }
}

# Texture size configs: (TexSize=source archive, MaxSize=downscale target, Downscaled flag)
$texConfigs = @(
    @{ TexSize = 512; MaxSize = 128; Downscaled = $true },
    @{ TexSize = 256; MaxSize = 0;   Downscaled = $false },
    @{ TexSize = 512; MaxSize = 0;   Downscaled = $false }
)

Write-Host "=== All-in-one d2x-xl mod conversion ==="
Write-Host ""

foreach ($game in @("d1", "d2")) {
    foreach ($cfg in $texConfigs) {
        $sz = $cfg.TexSize
        $mx = $cfg.MaxSize
        $label = if ($mx -gt 0) { $mx } else { $sz }
        $packStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $readme = Get-ReadmeText -GameId $game -Size $label -Downscaled $cfg.Downscaled
        # Always pass ImageMagick: strip splitting needs it even without downscaling
        $magickArg = if ($Magick) { $Magick } else { "" }
        Write-Host "--- Textures: $game ${label}x${label} ---"
        & $texScript -Game $game -TexSize $sz -MaxSize $mx -OutputDir $OutputDir -SevenZip $SevenZip -ReadmeText $readme -Magick $magickArg -Etc2Tool $Etc2Tool
        if ($LASTEXITCODE -ne 0) {
            Write-Host "WARNING: Texture conversion failed for $game $label"
        } else {
            Write-Host "--- Textures complete: $game ${label}x${label} in $(Format-ElapsedText $packStopwatch.Elapsed) ---"
        }
        Write-Host ""
    }
}

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
        [string]$DxaPath,
        [string[]]$Etc2Files,
        [string]$Indent = "    "
    )

    $updateStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "${Indent}Updating $(Split-Path $DxaPath -Leaf) with $($Etc2Files.Count) files"
    $zip = [System.IO.Compression.ZipFile]::Open($DxaPath, [System.IO.Compression.ZipArchiveMode]::Update)
    $fileIndex = 0
    foreach ($f in $Etc2Files) {
        $fileIndex++
        $entryName = [System.IO.Path]::GetFileName($f)
        # Restore '#' from safe naming used during conversion
        $entryName = $entryName -replace '_H_', '#'
        if ($fileIndex -le 3 -or $fileIndex % 25 -eq 0 -or $fileIndex -eq $Etc2Files.Count) {
            Write-Host "${Indent}[$((Format-ElapsedText $updateStopwatch.Elapsed))] Adding $fileIndex / $($Etc2Files.Count): $entryName"
        }
        $entry = $zip.CreateEntry($entryName, [System.IO.Compression.CompressionLevel]::NoCompression)
        $stream = $entry.Open()
        $fileBytes = [System.IO.File]::ReadAllBytes($f)
        $stream.Write($fileBytes, 0, $fileBytes.Length)
        $stream.Close()
    }
    $zip.Dispose()
    Write-Host "${Indent}Archive update complete in $(Format-ElapsedText $updateStopwatch.Elapsed)"
}

function Convert-AndAdd {
    param(
        [string]$GameId,
        [string]$ArchivePath,     # source 7z to extract from
        [string[]]$MissingNames,  # texture basenames to process
        [hashtable[]]$Targets     # @{DxaPath, MaxDim} -- packs to add results to
    )

    # IM writes warnings to stderr; prevent $ErrorActionPreference from
    # turning those into terminating errors (PS 5.1 quirk)
    $ErrorActionPreference = 'Continue'
    $totalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    $tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "dxx_merge_${GameId}_$(Get-Random)"
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null

    $extractDir = Join-Path $tempDir "extract"
    Invoke-7ZipExtract -SevenZipPath $SevenZip -ArchivePath $ArchivePath -ExtractDir $extractDir -Indent "  "
    $tgaDir = Join-Path (Join-Path $extractDir "textures") $GameId

    # Apply d2x-xl -> DXX name fixups (same as convert_d2xxl_textures.ps1)
    # Dot-source is impractical here, so apply the critical renames inline.
    # See Rename-D2xxlTextures in convert_d2xxl_textures.ps1 for full docs.
    $fixupStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "  Scanning merge texture name fixups"
    $renamed = 0

    # Boss animation frames: boss2_NN -> boss02#(NN-1) (512px D2 archive)
    foreach ($f in (Get-ChildItem -Path $tgaDir -Filter "boss2_*.tga" -File -ErrorAction SilentlyContinue)) {
        $bn = [IO.Path]::GetFileNameWithoutExtension($f.Name)
        if ($bn -match '^boss2_(\d+)$') {
            $newName = "boss02#$([int]$Matches[1] - 1).tga"
            $newPath = Join-Path $tgaDir $newName
            if (-not (Test-Path $newPath)) {
                Rename-Item $f.FullName $newName
                $renamed++
            }
        }
    }
    # Exclamation mark frame syntax: name!N -> name#N (256px D2 archive)
    foreach ($f in (Get-ChildItem -Path $tgaDir -Filter "*!*.tga" -File -ErrorAction SilentlyContinue)) {
        $newName = $f.Name -replace '!', '#'
        $newPath = Join-Path $tgaDir $newName
        if (-not (Test-Path $newPath)) {
            Rename-Item $f.FullName $newName
            $renamed++
        }
    }
    # Targeting reticle: rename -green variants to base name, leave -red as-is
    foreach ($f in (Get-ChildItem -Path $tgaDir -Filter "targ*-green.tga" -File -ErrorAction SilentlyContinue)) {
        $newName = $f.Name -replace '-green\.tga$', '.tga'
        $newPath = Join-Path $tgaDir $newName
        if (Test-Path $newPath) { Remove-Item $newPath -Force }
        Rename-Item $f.FullName $newName
        $renamed++
    }
    # D1-only names, base names without frame suffix, addon textures:
    # left as-is in archive (unmatched but harmless)
    Write-Host "  Merge name fixup scan complete: $renamed renamed in $(Format-ElapsedText $fixupStopwatch.Elapsed)"

    # Pre-split animation strips so individual frames are available
    $splitStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "  Scanning merge animation strips"
    $splitCount = 0
    if ($Magick -and (Test-Path $Magick)) {
        foreach ($tga in (Get-ChildItem -Path $tgaDir -Filter "*#0*.tga" -File -ErrorAction SilentlyContinue)) {
            # Use __STRIP__ prefix so strip source won't collide with frame 0's safe name
            $safeName = "__STRIP__$($tga.Name -replace '#', '__H__')"
            $safePath = Join-Path $tgaDir $safeName
            Copy-Item $tga.FullName $safePath -Force
            $dims = & $Magick identify -format "%w %h" $safePath 2>$null
            if ($LASTEXITCODE -ne 0 -or -not $dims) { Remove-Item $safePath -Force; continue }
            $parts = ("$dims" -split '\s+'); $w = [int]$parts[0]; $h = [int]$parts[1]
            if ($h -le $w -or $h % $w -ne 0 -or ($h / $w) -le 1) {
                Remove-Item $safePath -Force; continue
            }
            $bn = [IO.Path]::GetFileNameWithoutExtension($tga.Name)
            $variant = ""; $nameBase = $bn
            if ($bn -match '^(.+)#0(-\w+)$') { $nameBase = $Matches[1]; $variant = $Matches[2] }
            else { $nameBase = $bn -replace '#0$', '' }
            # Remove original strip before splitting -- we work from the safe
            # copy, and frame 0 gets the same filename as the original strip
            Remove-Item $tga.FullName -Force
            $splitCount++
            for ($i = 0; $i -lt ($h / $w); $i++) {
                $frameName = "${nameBase}#${i}${variant}.tga"
                $safeFrame = Join-Path $tgaDir ($frameName -replace '#', '__H__')
                & $Magick $safePath -crop "${w}x${w}+0+$($i * $w)" +repage $safeFrame 2>$null
                if ($LASTEXITCODE -eq 0) {
                    $realFrame = Join-Path $tgaDir $frameName
                    if (Test-Path $realFrame) { Remove-Item $realFrame -Force }
                    Rename-Item $safeFrame $frameName
                }
            }
            Remove-Item $safePath -Force -ErrorAction SilentlyContinue
        }
    }
    Write-Host "  Merge animation strip scan complete: $splitCount split in $(Format-ElapsedText $splitStopwatch.Elapsed)"

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
        Write-Host "    Target $label: processing $mergeTotal textures"
        foreach ($name in $MissingNames) {
            $processed++
            Write-ItemStartLine -Stopwatch $targetStopwatch -Index $processed -Total $mergeTotal -ItemName $name -Indent "      "
            $src = Join-Path $tgaDir "$name.tga"
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
                # Skip downscaling for HUD/cockpit textures
                $effectiveMaxDim = if ($maxDim -gt 0 -and $name -match $noDownscalePattern) { 0 } else { $maxDim }
                if ($effectiveMaxDim -gt 0 -and $Magick) {
                    # Copy to safe name to avoid IM interpreting '#' as scene selector
                    $safeSrc = Join-Path $outDir "${safeName}_src$([IO.Path]::GetExtension($src))"
                    Copy-Item $src $safeSrc -Force
                    $tempPng = Join-Path $outDir "$safeName.png"
                    & $Magick $safeSrc '-colorspace' 'RGB' '-filter' 'Lanczos' `
                        '-resize' "${effectiveMaxDim}x${effectiveMaxDim}" '-unsharp' '0x0.4' `
                        '-colorspace' 'sRGB' $tempPng 2>$null
                    if ($LASTEXITCODE -ne 0) { throw "magick failed" }
                    & $Etc2Tool $tempPng $tempEtc2 2>$null
                    Remove-Item $safeSrc, $tempPng -ErrorAction SilentlyContinue
                } else {
                    & $Etc2Tool $src $tempEtc2 2>$null
                }
                if ($LASTEXITCODE -ne 0) { throw "etc2tool failed" }
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

        if ($etc2Files.Count -gt 0) {
            Write-Host "    Adding $converted textures to $(Split-Path $dxaPath -Leaf)"
            Add-Etc2ToDxa -DxaPath $dxaPath -Etc2Files $etc2Files -Indent "      "
        } else {
            Write-Host "    No converted textures for $(Split-Path $dxaPath -Leaf)"
        }

        Write-Host "    Target $label complete: processed $processed / $mergeTotal, converted $converted, errors $errors in $(Format-ElapsedText $targetStopwatch.Elapsed)"
    }

    Remove-Item -Recurse -Force $tempDir
    Write-Host "  Merge source $GameId complete in $(Format-ElapsedText $totalStopwatch.Elapsed)"
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
$soundStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
& $sndScript -Game both -OutputDir $OutputDir -SevenZip $SevenZip
if ($LASTEXITCODE -ne 0) {
    Write-Host "WARNING: Sound conversion failed"
} else {
    Write-Host "--- Sounds complete in $(Format-ElapsedText $soundStopwatch.Elapsed) ---"
}

Write-Host ""
Write-Host "=== All conversions complete ==="
Write-Host "Output directory: $OutputDir"
Get-ChildItem -Path $OutputDir -Filter "*-hires-*.dxa" | ForEach-Object {
    $sizeMB = [Math]::Round($_.Length / 1MB, 1)
    Write-Host "  $($_.Name) ($sizeMB MB)"
}
