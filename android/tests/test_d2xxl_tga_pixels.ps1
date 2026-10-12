#!/usr/bin/env pwsh
# Exercise production TGA decoding without allocating platform-specific bitmaps
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $repoRoot 'game_data/mods/d2x-xl/convert_d2xxl_textures.ps1')
$root = Join-Path $repoRoot ('android/temp/d2xxl_tga_pixels/run_' + [guid]::NewGuid().ToString('N'))
& (Join-Path $repoRoot 'android/helpers/retain-recent-artifacts.ps1') -Artifacts $root -DirectoryPrefix run_
New-Item -ItemType Directory -Path $root -Force | Out-Null
$producerLock = [IO.File]::Open((Join-Path $root 'producer.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $path = Join-Path $root 'pixels.tga'
    $canonical = @([byte[]](1, 2, 3, 255), [byte[]](4, 5, 6, 128), [byte[]](7, 8, 9, 255), [byte[]](10, 11, 12, 255))
    foreach ($channels in @(3, 4)) {
        foreach ($origin in @(0, 16, 32, 48)) {
            $header = [byte[]]::new(18)
            $header[2] = 2
            $header[12] = 2
            $header[14] = 2
            $header[16] = $channels * 8
            $header[17] = $origin -bor $(if ($channels -eq 4) { 8 } else { 0 })
            $bytes = [Collections.Generic.List[byte]]::new()
            $bytes.AddRange($header)
            foreach ($y in 0..1) {
                foreach ($x in 0..1) {
                    $canonicalY = if ($origin -band 32) { $y } else { 1 - $y }
                    $canonicalX = if ($origin -band 16) { 1 - $x } else { $x }
                    $bytes.AddRange([byte[]]$canonical[$canonicalY * 2 + $canonicalX][0..($channels - 1)])
                }
            }
            [IO.File]::WriteAllBytes($path, $bytes.ToArray())
            $decoded = Read-D2xxlTgaPixels -Path $path
            if ($decoded.Width -ne 2 -or $decoded.Height -ne 2 -or $decoded.Channels -ne $channels -or $decoded.Mask) { throw 'Incorrect decoded layout' }
            foreach ($index in 0..3) {
                foreach ($channel in 0..($channels - 1)) {
                    if ($decoded.Pixels[$index * $channels + $channel] -ne $canonical[$index][$channel]) { throw "Incorrect pixel: depth=$channels origin=$origin pixel=$index channel=$channel" }
                }
            }
        }
    }
    # Last fixture is 32-bit, top-right origin; replace its first stored pixel
    $keyed = $bytes.ToArray()
    [Array]::Copy([byte[]](128, 88, 120, 255), 0, $keyed, 18, 4)
    [IO.File]::WriteAllBytes($path, $keyed)
    $decoded = Read-D2xxlTgaPixels -Path $path
    if ($decoded.Pixels[7] -ne 0 -or $decoded.Mask.Length -ne 16 -or $decoded.Mask[7] -ne 0 -or $decoded.Mask[3] -ne 255) { throw 'Key color or mask decoding failed' }
    $opaque = $bytes.ToArray()
    $opaque[17] = 48
    [IO.File]::WriteAllBytes($path, $opaque)
    $decoded = Read-D2xxlTgaPixels -Path $path
    if ($decoded.Pixels[7] -ne 255) { throw 'Zero descriptor alpha bits did not force opaque pixels' }
    $decoded = Read-D2xxlTgaPixels -Path $path -D2xxlSource
    if ($decoded.Pixels[7] -ne $canonical[1][3]) { throw 'XL legacy source alpha was discarded' }
    $originalBytes = $bytes.ToArray()
    $rleHeader = [byte[]]$originalBytes[0..17]
    $rleHeader[2] = 10
    [IO.File]::WriteAllBytes($path, [byte[]]($rleHeader + [byte]3 + $originalBytes[18..($originalBytes.Length - 1)]))
    $decoded = Read-D2xxlTgaPixels -Path $path
    foreach ($index in 0..3) {
        foreach ($channel in 0..3) {
            if ($decoded.Pixels[$index * 4 + $channel] -ne $canonical[$index][$channel]) { throw 'Raw RLE packet changed oriented pixels' }
        }
    }
    $rleHeader[17] = 40
    $mixedPackets = [byte[]]($rleHeader + [byte]1 + $canonical[0] + $canonical[1] + [byte]129 + $canonical[3])
    [IO.File]::WriteAllBytes($path, $mixedPackets)
    $decoded = Read-D2xxlTgaPixels -Path $path
    foreach ($index in 0..3) {
        $expected = $canonical[$(if ($index -ge 2) { 3 } else { $index })]
        foreach ($channel in 0..3) {
            if ($decoded.Pixels[$index * 4 + $channel] -ne $expected[$channel]) { throw 'Mixed RLE packet changed pixels' }
        }
    }
    foreach ($invalidRle in @([byte[]]$rleHeader, [byte[]]($rleHeader + [byte]132 + $canonical[0]), [byte[]]($rleHeader + [byte]131 + [byte[]](1, 2)))) {
        [IO.File]::WriteAllBytes($path, $invalidRle)
        $rejected = $false
        try { Read-D2xxlTgaPixels -Path $path | Out-Null } catch { $rejected = $true }
        if (!$rejected) { throw 'Invalid RLE packet was accepted' }
    }
    foreach ($invalid in @([byte[]]$opaque[0..10], [byte[]]$opaque[0..($opaque.Length - 2)], [byte[]]($opaque + [byte]1))) {
        [IO.File]::WriteAllBytes($path, $invalid)
        $rejected = $false
        try { Read-D2xxlTgaPixels -Path $path | Out-Null } catch { $rejected = $true }
        if (-not $rejected) { throw 'Invalid TGA was accepted' }
    }
    Write-Host 'PASS: portable TGA depth, origin, alpha, key mask and malformed input checks'
} finally {
    $producerLock.Dispose()
    Remove-Item -LiteralPath $root -Recurse -Force
}
