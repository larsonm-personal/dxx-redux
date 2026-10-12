#!/usr/bin/env pwsh
# Original source strips must cover the game's entire animation, including its final frame
[CmdletBinding()]
param([string]$SevenZip = 'C:/Program Files/7-Zip/7z.exe')
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot)
$source = Join-Path $repo 'game_data/mods/d2x-xl'
. (Join-Path $source 'convert_d2xxl_textures.ps1') -SevenZip $SevenZip
$root = Join-Path $repo ('android/temp/d2xxl-animation-test/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repo 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
New-Item -ItemType Directory -Path $root -Force | Out-Null
function Get-PixelHash([Drawing.Bitmap]$Bitmap) {
    $rect = [Drawing.Rectangle]::new(0, 0, $Bitmap.Width, $Bitmap.Height)
    $data = $Bitmap.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        if ($data.Stride -ne $Bitmap.Width * 4) { throw 'Unexpected fixture bitmap stride' }
        $bytes = [byte[]]::new($data.Stride * $Bitmap.Height)
        [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
        return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes))
    } finally { $Bitmap.UnlockBits($data) }
}
$cases = @(
    @{ game = 'd1'; name = 'door18'; frames = @(0, 1, 2, 3, 5, 6, 7, 8) }
    @{ game = 'd1'; name = 'door19'; frames = @(0, 1, 2, 3, 5, 6, 7, 8) }
    @{ game = 'd1'; name = 'door20'; frames = @(0, 1, 3, 4, 5, 7, 8) }
    @{ game = 'd1'; name = 'door21'; frames = @(0, 2, 3, 5, 6, 8) }
    @{ game = 'd1'; name = 'door23'; frames = @(0, 1, 3, 4, 5, 7, 8) }
    @{ game = 'd1'; name = 'door25'; frames = @(0, 1, 2, 4, 5, 6, 7) }
    @{ game = 'd1'; name = 'wall01'; frames = @(0, 4, 8, 11, 15) }
    @{ game = 'd1'; name = 'misc066'; frames = @(0, 1, 2, 3, 4, 6, 7, 8, 9) }
    @{ game = 'd1'; name = 'misc11'; frames = @(0, 1, 3, 4) }
    @{ game = 'd2'; name = 'door18'; frames = @(0, 1, 2, 3, 5, 6, 7, 8) }
    @{ game = 'd2'; name = 'door38'; frames = @(0, 2, 3, 5, 6, 8) }
    @{ game = 'd2'; name = 'wall01'; frames = @(0, 4, 8, 11, 15) }
    @{ game = 'd2'; name = 'lava06'; frames = @(0, 2, 4, 6) }
    @{ game = 'd2'; name = 'mtrl01'; frames = @(0, 1, 1, 2, 3, 3, 4, 5, 5, 6, 6, 7, 8, 8, 9, 10, 10, 11) }
    @{ game = 'd2'; name = 'mtrl03'; frames = @(0, 1, 3, 4, 6, 7, 8, 10, 11) }
    @{ game = 'd2'; name = 'box03b'; frames = @(0, 1) }
)
foreach ($case in $cases) {
    $game = $case.game
    $name = $case.name
    $directory = Join-Path $root "$game-$name"
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    & $SevenZip e (Join-Path $source "d2x-xl/$($game.ToUpper())-textures-512x512.7z") "textures/$game/$name#0.tga" "-o$directory" -y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Source extraction failed: $game $name" }
    $original = Read-TGA (Join-Path $directory "$name#0.tga")
    try {
        $width = $original.Bitmap.Width
        Split-StripTextures -TgaDir $directory -GameId $game | Out-Null
        $images = @(Get-ChildItem $directory -Filter '*.png' | Where-Object Name -NotLike '*_mask.png')
        if ($images.Count -ne $case.frames.Count) { throw "Incorrect frame count: $game $name" }
        for ($frame = 0; $frame -lt $case.frames.Count; $frame++) {
            $rect = [Drawing.Rectangle]::new(0, $case.frames[$frame] * $width, $width, $width)
            foreach ($kind in @('Bitmap', 'Mask')) {
                if (!$original.$kind) { continue }
                $suffix = if ($kind -eq 'Mask') { '_mask' } else { '' }
                $expected = $original.$kind.Clone($rect, $original.$kind.PixelFormat)
                $actual = [Drawing.Bitmap]::new((Join-Path $directory "$name#$frame$suffix.png"))
                try {
                    if ($actual.Width -ne $width -or $actual.Height -ne $width -or (Get-PixelHash $actual) -ne (Get-PixelHash $expected)) {
                        throw "Incorrect original pixels in $game $name frame $frame $kind"
                    }
                } finally { $actual.Dispose(); $expected.Dispose() }
            }
        }
        Write-Host "Verified $game $name ($($case.frames.Count) frames)"
    } finally {
        $original.Bitmap.Dispose()
        if ($original.Mask) { $original.Mask.Dispose() }
    }
}
Write-Host "Original-source animation mapping passed: $root"
