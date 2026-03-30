<#
.SYNOPSIS
    Converts all d2x-xl texture and sound packs into .dxa mod files.

.DESCRIPTION
    Calls convert_d2xxl_textures.ps1 for each combination of game (d1/d2)
    and texture size (128/256/512), then convert_d2xxl_sounds.ps1 for both
    games. Produces 6 texture .dxa files and 2 sound .dxa files.

    Output files:
      d2xxl-hires-textures-d1-128.dxa
      d2xxl-hires-textures-d1-256.dxa
      d2xxl-hires-textures-d1-512.dxa
      d2xxl-hires-textures-d2-128.dxa
      d2xxl-hires-textures-d2-256.dxa
      d2xxl-hires-textures-d2-512.dxa
      d2xxl-hires-sounds-d1.dxa
      d2xxl-hires-sounds-d2.dxa

.PARAMETER OutputDir
    Output directory for .dxa files. Default: same directory as this script

.PARAMETER SevenZip
    Path to 7z.exe. Default: "C:\Program Files\7-Zip\7z.exe"
#>
param(
    [string]$OutputDir = "",
    [string]$SevenZip = "C:\Program Files\7-Zip\7z.exe",
    [string]$Magick = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $OutputDir) { $OutputDir = $scriptDir }

$texScript = Join-Path $scriptDir "convert_d2xxl_textures.ps1"
$sndScript = Join-Path $scriptDir "convert_d2xxl_sounds.ps1"

# Auto-locate ImageMagick if not provided
if (-not $Magick) {
    $repoRoot = Split-Path (Split-Path $scriptDir)
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
    $packName = "d2xxl-hires-textures-${GameId}-${Size}"
    if ($Downscaled) {
        @"
# ${packName}

${Size}x${Size} replacement textures for ${gameName}

Downscaled from the 512x512 d2x-xl texture pack using ImageMagick
with linear-light colorspace conversion, Lanczos resampling, and
micro-sharpening to preserve detail at reduced resolution

Original textures from d2x-xl by ${credits}
"@
    } else {
        @"
# ${packName}

${Size}x${Size} high-resolution replacement textures for ${gameName}

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
        $readme = Get-ReadmeText -GameId $game -Size $label -Downscaled $cfg.Downscaled
        $magickArg = if ($cfg.Downscaled -and $Magick) { $Magick } else { "" }
        Write-Host "--- Textures: $game ${label}x${label} ---"
        & $texScript -Game $game -TexSize $sz -MaxSize $mx -OutputDir $OutputDir -SevenZip $SevenZip -ReadmeText $readme -Magick $magickArg
        if ($LASTEXITCODE -ne 0) {
            Write-Host "WARNING: Texture conversion failed for $game $label"
        }
        Write-Host ""
    }
}

# Sounds: both games
Write-Host "--- Sounds: d1 + d2 ---"
& $sndScript -Game both -OutputDir $OutputDir -SevenZip $SevenZip
if ($LASTEXITCODE -ne 0) {
    Write-Host "WARNING: Sound conversion failed"
}

Write-Host ""
Write-Host "=== All conversions complete ==="
Write-Host "Output directory: $OutputDir"
Get-ChildItem -Path $OutputDir -Filter "d2xxl-hires-*.dxa" | ForEach-Object {
    $sizeMB = [Math]::Round($_.Length / 1MB, 1)
    Write-Host "  $($_.Name) ($sizeMB MB)"
}
