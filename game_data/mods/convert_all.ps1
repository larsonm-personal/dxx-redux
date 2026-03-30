<#
.SYNOPSIS
    Converts all d2x-xl texture and sound packs into .dxa mod files.

.DESCRIPTION
    Calls convert_d2xxl_textures.ps1 for each combination of game (d1/d2)
    and texture size (256/512), then convert_d2xxl_sounds.ps1 for both
    games. Produces 4 texture .dxa files and 2 sound .dxa files.

    Output files:
      d2xxl-hires-textures-d1-256.dxa
      d2xxl-hires-textures-d1-512.dxa
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
    [string]$SevenZip = "C:\Program Files\7-Zip\7z.exe"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $OutputDir) { $OutputDir = $scriptDir }

$texScript = Join-Path $scriptDir "convert_d2xxl_textures.ps1"
$sndScript = Join-Path $scriptDir "convert_d2xxl_sounds.ps1"

Write-Host "=== All-in-one d2x-xl mod conversion ==="
Write-Host ""

# Textures: 4 combinations (d1/d2 x 256/512)
foreach ($game in @("d1", "d2")) {
    foreach ($size in @(256, 512)) {
        Write-Host "--- Textures: $game ${size}x${size} ---"
        & $texScript -Game $game -TexSize $size -OutputDir $OutputDir -SevenZip $SevenZip
        if ($LASTEXITCODE -ne 0) {
            Write-Host "WARNING: Texture conversion failed for $game $size"
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
