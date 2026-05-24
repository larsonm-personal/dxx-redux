#!/usr/bin/env pwsh
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Drawing

$script:XfingBitmapFlagTransparent = 1
$script:XfingBitmapFlagSuperTransparent = 2
$script:XfingBitmapFlagRle = 8
$script:XfingBitmapFlagRleBig = 32

function Get-XfingSha256ForBytes {
    param(
        [byte[]]$Bytes,
        [int]$Offset = 0,
        [int]$Length = -1
    )

    if ($Length -lt 0) {
        $Length = $Bytes.Length - $Offset
    }
    if ($Offset -lt 0 -or $Length -lt 0 -or ($Offset + $Length) -gt $Bytes.Length) {
        throw "Invalid hash range offset=$Offset length=$Length bytes=$($Bytes.Length)"
    }

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($Bytes, $Offset, $Length))).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Get-XfingSha256ForFile {
    param([string]$Path)

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Read-XfingFixedAscii {
    param(
        [System.IO.BinaryReader]$Reader,
        [int]$Length
    )

    $bytes = $Reader.ReadBytes($Length)
    $text = [System.Text.Encoding]::ASCII.GetString($bytes)
    $zeroIndex = $text.IndexOf([char]0)
    if ($zeroIndex -ge 0) {
        $text = $text.Substring(0, $zeroIndex)
    }
    return $text.TrimEnd()
}

function Get-XfingBitmapName {
    param(
        [string]$Name,
        [int]$DFlags
    )

    if (($DFlags -band 64) -ne 0) {
        return "$Name#$($DFlags -band 63)"
    }
    return $Name
}

function Get-XfingSafeName {
    param([string]$Name)

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return "unnamed"
    }
    return ($Name -replace '[^A-Za-z0-9._#+-]', '_')
}

function Read-XfingPaletteBytes {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 768) {
        throw "Palette file too small: $Path"
    }
    $palette = [byte[]]::new(768)
    [Array]::Copy($bytes, 0, $palette, 0, 768)
    return $palette
}

function Convert-XfingPaletteComponent {
    param([byte]$Value)

    return [byte]([Math]::Min(255, [int]$Value * 4))
}

function Expand-XfingRleRow {
    param(
        [byte[]]$Bytes,
        [int]$SourceOffset,
        [byte[]]$Destination,
        [int]$DestinationOffset,
        [int]$Width
    )

    $source = $SourceOffset
    $dest = $DestinationOffset
    $destEnd = $DestinationOffset + $Width
    while ($dest -lt $destEnd) {
        $data = $Bytes[$source]
        $source++
        if (($data -band 0xE0) -ne 0xE0) {
            $Destination[$dest] = $data
            $dest++
        } else {
            $count = $data -band 31
            if ($count -eq 0) {
                break
            }
            $color = $Bytes[$source]
            $source++
            for ($index = 0; $index -lt $count -and $dest -lt $destEnd; $index++) {
                $Destination[$dest] = $color
                $dest++
            }
        }
    }
    if ($dest -ne $destEnd) {
        throw "RLE row decoded to $($dest - $DestinationOffset) pixels, expected $Width"
    }
}

function Expand-XfingBitmapPixels {
    param(
        $Pig,
        $Entry
    )

    $size = [int]$Entry.Width * [int]$Entry.Height
    $pixels = [byte[]]::new($size)
    if (($Entry.Flags -band $script:XfingBitmapFlagRle) -ne 0) {
        $rowSizeBytes = if (($Entry.Flags -band $script:XfingBitmapFlagRleBig) -ne 0) { 2 } else { 1 }
        $dataOffset = [int]$Entry.Offset + 4 + ([int]$Entry.Height * $rowSizeBytes)
        $sourceOffset = $dataOffset
        for ($row = 0; $row -lt [int]$Entry.Height; $row++) {
            $rowSizeOffset = [int]$Entry.Offset + 4 + ($row * $rowSizeBytes)
            $rowSize = if ($rowSizeBytes -eq 2) {
                [BitConverter]::ToUInt16($Pig.Bytes, $rowSizeOffset)
            } else {
                [int]$Pig.Bytes[$rowSizeOffset]
            }
            Expand-XfingRleRow `
                -Bytes $Pig.Bytes `
                -SourceOffset $sourceOffset `
                -Destination $pixels `
                -DestinationOffset ($row * [int]$Entry.Width) `
                -Width ([int]$Entry.Width)
            $sourceOffset += $rowSize
        }
    } else {
        [Array]::Copy($Pig.Bytes, [int]$Entry.Offset, $pixels, 0, $size)
    }
    return $pixels
}

function Write-XfingRgbaPng {
    param(
        [string]$Path,
        [int]$Width,
        [int]$Height,
        [byte[]]$Rgba
    )

    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }

    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $rect = [System.Drawing.Rectangle]::new(0, 0, $Width, $Height)
        $data = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, $bitmap.PixelFormat)
        try {
            $stride = $data.Stride
            $bytes = [byte[]]::new($stride * $Height)
            for ($y = 0; $y -lt $Height; $y++) {
                for ($x = 0; $x -lt $Width; $x++) {
                    $src = (($y * $Width) + $x) * 4
                    $dst = ($y * $stride) + ($x * 4)
                    $bytes[$dst] = $Rgba[$src + 2]
                    $bytes[$dst + 1] = $Rgba[$src + 1]
                    $bytes[$dst + 2] = $Rgba[$src]
                    $bytes[$dst + 3] = $Rgba[$src + 3]
                }
            }
            [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $data.Scan0, $bytes.Length)
        } finally {
            $bitmap.UnlockBits($data)
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $bitmap.Dispose()
    }
}

function Export-XfingBitmapEntryPng {
    param(
        $Pig,
        $Entry,
        [byte[]]$Palette,
        [string]$OutputPath,
        [string]$MaskOutputPath = ""
    )

    $pixels = Expand-XfingBitmapPixels -Pig $Pig -Entry $Entry
    $rgba = [byte[]]::new([int]$Entry.Width * [int]$Entry.Height * 4)
    $maskRgba = [byte[]]::new($rgba.Length)
    $hasSuperTransparent = $false
    for ($index = 0; $index -lt $pixels.Length; $index++) {
        $color = [int]$pixels[$index]
        $out = $index * 4
        $alpha = [byte]255
        $red = Convert-XfingPaletteComponent $Palette[$color * 3]
        $green = Convert-XfingPaletteComponent $Palette[$color * 3 + 1]
        $blue = Convert-XfingPaletteComponent $Palette[$color * 3 + 2]
        if (($Entry.Flags -band $script:XfingBitmapFlagTransparent) -ne 0 -and $color -eq 255) {
            $red = 0
            $green = 0
            $blue = 0
            $alpha = 0
        } elseif (($Entry.Flags -band $script:XfingBitmapFlagSuperTransparent) -ne 0 -and $color -eq 254) {
            $red = 120
            $green = 88
            $blue = 128
            $alpha = 0
            $hasSuperTransparent = $true
        }
        $rgba[$out] = $red
        $rgba[$out + 1] = $green
        $rgba[$out + 2] = $blue
        $rgba[$out + 3] = $alpha

        if ($color -eq 254 -and ($Entry.Flags -band $script:XfingBitmapFlagSuperTransparent) -ne 0) {
            $maskRgba[$out] = 0
            $maskRgba[$out + 1] = 0
            $maskRgba[$out + 2] = 0
            $maskRgba[$out + 3] = 255
        } else {
            $maskRgba[$out] = 255
            $maskRgba[$out + 1] = 255
            $maskRgba[$out + 2] = 255
            $maskRgba[$out + 3] = 255
        }
    }

    Write-XfingRgbaPng -Path $OutputPath -Width ([int]$Entry.Width) -Height ([int]$Entry.Height) -Rgba $rgba
    if ($hasSuperTransparent -and $MaskOutputPath) {
        Write-XfingRgbaPng -Path $MaskOutputPath -Width ([int]$Entry.Width) -Height ([int]$Entry.Height) -Rgba $maskRgba
    }
    return [pscustomobject]@{
        path = $OutputPath
        width = [int]$Entry.Width
        height = [int]$Entry.Height
        hasTransparency = (($Entry.Flags -band $script:XfingBitmapFlagTransparent) -ne 0)
        hasSuperTransparency = $hasSuperTransparent
        sha256 = Get-XfingSha256ForFile $OutputPath
        maskSha256 = if ($hasSuperTransparent -and $MaskOutputPath) { Get-XfingSha256ForFile $MaskOutputPath } else { $null }
    }
}

function Get-XfingNextOffset {
    param(
        [int[]]$SortedOffsets,
        [int]$Offset,
        [int]$FileLength
    )

    foreach ($candidate in $SortedOffsets) {
        if ($candidate -gt $Offset) {
            return $candidate
        }
    }
    return $FileLength
}

function Read-XfingD1Pig {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $stream = [System.IO.MemoryStream]::new($bytes)
    $reader = [System.IO.BinaryReader]::new($stream)
    $fileLength = $bytes.Length

    switch ($fileLength) {
        5092871 { $pigDataStart = 0 }
        2529454 { $pigDataStart = 0 }
        2509799 { $pigDataStart = 0 }
        7640220 { $pigDataStart = 0 }
        4520145 { $pigDataStart = 0 }
        default { $pigDataStart = $reader.ReadInt32() }
    }

    $stream.Position = $pigDataStart
    $bitmapCount = $reader.ReadInt32()
    $soundCount = $reader.ReadInt32()
    $bitmapHeaders = @()
    for ($entryIndex = 0; $entryIndex -lt $bitmapCount; $entryIndex++) {
        $rawName = Read-XfingFixedAscii -Reader $reader -Length 8
        $dflags = $reader.ReadByte()
        $widthByte = $reader.ReadByte()
        $heightByte = $reader.ReadByte()
        $flags = $reader.ReadByte()
        $avgColor = $reader.ReadByte()
        $relativeOffset = $reader.ReadInt32()
        $bitmapHeaders += [pscustomobject]@{
            Index = $entryIndex + 1
            Name = Get-XfingBitmapName -Name $rawName -DFlags $dflags
            RawName = $rawName
            DFlags = $dflags
            Width = if (($dflags -band 128) -ne 0) { $widthByte + 256 } else { $widthByte }
            Height = $heightByte
            Flags = $flags
            AvgColor = $avgColor
            RelativeOffset = $relativeOffset
        }
    }

    $soundHeaders = @()
    for ($entryIndex = 0; $entryIndex -lt $soundCount; $entryIndex++) {
        $soundName = Read-XfingFixedAscii -Reader $reader -Length 8
        $length = $reader.ReadInt32()
        $dataLength = $reader.ReadInt32()
        $relativeOffset = $reader.ReadInt32()
        $soundHeaders += [pscustomobject]@{
            Name = $soundName
            Length = $length
            DataLength = $dataLength
            RelativeOffset = $relativeOffset
        }
    }

    $dataBase = $pigDataStart + 8 + ($bitmapCount * 17) + ($soundCount * 20)
    $cutpoints = New-Object System.Collections.Generic.List[int]
    foreach ($header in $bitmapHeaders) {
        $cutpoints.Add($dataBase + $header.RelativeOffset)
    }
    foreach ($header in $soundHeaders) {
        $cutpoints.Add($dataBase + $header.RelativeOffset)
    }
    $sortedOffsets = $cutpoints.ToArray() | Sort-Object -Unique

    $entries = foreach ($header in $bitmapHeaders) {
        $absoluteOffset = $dataBase + $header.RelativeOffset
        $nextOffset = Get-XfingNextOffset -SortedOffsets $sortedOffsets -Offset $absoluteOffset -FileLength $fileLength
        $payloadLength = $nextOffset - $absoluteOffset
        [pscustomobject]@{
            Name = $header.Name
            RawName = $header.RawName
            Index = $header.Index
            Width = $header.Width
            Height = $header.Height
            Flags = $header.Flags
            DFlags = $header.DFlags
            AvgColor = $header.AvgColor
            Offset = $absoluteOffset
            Length = $payloadLength
            Hash = Get-XfingSha256ForBytes -Bytes $bytes -Offset $absoluteOffset -Length $payloadLength
        }
    }

    $reader.Dispose()
    $stream.Dispose()
    return [pscustomobject]@{
        Path = $Path
        Format = "d1pig"
        PigDataStart = $pigDataStart
        Bytes = $bytes
        BitmapCount = $bitmapCount
        SoundCount = $soundCount
        Entries = @($entries)
        Sha256 = Get-XfingSha256ForBytes -Bytes $bytes
    }
}

function Read-XfingD2Pig {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $stream = [System.IO.MemoryStream]::new($bytes)
    $reader = [System.IO.BinaryReader]::new($stream)
    $magic = $reader.ReadInt32()
    $version = $reader.ReadInt32()
    if ($magic -ne 1195987024 -or $version -ne 2) {
        throw "Not a supported D2 PIG: $Path magic=$magic version=$version"
    }
    $bitmapCount = $reader.ReadInt32()
    $headers = @()
    for ($entryIndex = 0; $entryIndex -lt $bitmapCount; $entryIndex++) {
        $rawName = Read-XfingFixedAscii -Reader $reader -Length 8
        $dflags = $reader.ReadByte()
        $widthByte = $reader.ReadByte()
        $heightByte = $reader.ReadByte()
        $whExtra = $reader.ReadByte()
        $flags = $reader.ReadByte()
        $avgColor = $reader.ReadByte()
        $relativeOffset = $reader.ReadInt32()
        $headers += [pscustomobject]@{
            Index = $entryIndex + 1
            Name = Get-XfingBitmapName -Name $rawName -DFlags $dflags
            RawName = $rawName
            DFlags = $dflags
            Width = $widthByte + (($whExtra -band 0x0f) -shl 8)
            Height = $heightByte + (($whExtra -band 0xf0) -shl 4)
            Flags = $flags
            AvgColor = $avgColor
            RelativeOffset = $relativeOffset
        }
    }

    $dataBase = 12 + ($bitmapCount * 18)
    $sortedOffsets = ($headers | ForEach-Object { $dataBase + $_.RelativeOffset }) | Sort-Object -Unique
    $fileLength = $bytes.Length
    $entries = foreach ($header in $headers) {
        $absoluteOffset = $dataBase + $header.RelativeOffset
        $nextOffset = Get-XfingNextOffset -SortedOffsets $sortedOffsets -Offset $absoluteOffset -FileLength $fileLength
        $payloadLength = $nextOffset - $absoluteOffset
        [pscustomobject]@{
            Name = $header.Name
            RawName = $header.RawName
            Index = $header.Index
            Width = $header.Width
            Height = $header.Height
            Flags = $header.Flags
            DFlags = $header.DFlags
            AvgColor = $header.AvgColor
            Offset = $absoluteOffset
            Length = $payloadLength
            Hash = Get-XfingSha256ForBytes -Bytes $bytes -Offset $absoluteOffset -Length $payloadLength
        }
    }

    $reader.Dispose()
    $stream.Dispose()
    return [pscustomobject]@{
        Path = $Path
        Format = "d2pig"
        Bytes = $bytes
        BitmapCount = $bitmapCount
        SoundCount = 0
        Entries = @($entries)
        Sha256 = Get-XfingSha256ForBytes -Bytes $bytes
    }
}

function Compare-XfingPigEntriesByIndex {
    param(
        $BasePig,
        $PatchPig
    )

    $maxCount = [Math]::Max($BasePig.Entries.Count, $PatchPig.Entries.Count)
    $rows = @()
    for ($index = 0; $index -lt $maxCount; $index++) {
        $baseEntry = if ($index -lt $BasePig.Entries.Count) { $BasePig.Entries[$index] } else { $null }
        $patchEntry = if ($index -lt $PatchPig.Entries.Count) { $PatchPig.Entries[$index] } else { $null }
        $status = "Same"
        if ($null -eq $baseEntry) {
            $status = "Extra"
        } elseif ($null -eq $patchEntry) {
            $status = "Missing"
        } elseif ($baseEntry.Hash -ne $patchEntry.Hash -or
            $baseEntry.Name -ne $patchEntry.Name -or
            $baseEntry.RawName -ne $patchEntry.RawName -or
            $baseEntry.Width -ne $patchEntry.Width -or
            $baseEntry.Height -ne $patchEntry.Height -or
            $baseEntry.Flags -ne $patchEntry.Flags -or
            $baseEntry.DFlags -ne $patchEntry.DFlags -or
            $baseEntry.AvgColor -ne $patchEntry.AvgColor -or
            $baseEntry.Length -ne $patchEntry.Length) {
            $status = "Changed"
        }

        $rows += [pscustomobject]@{
            Status = $status
            Base = $baseEntry
            Patch = $patchEntry
        }
    }
    return @($rows)
}

function Write-XfingPayloadFile {
    param(
        [byte[]]$Bytes,
        $Entry,
        [string]$OutputPath
    )

    $parent = Split-Path -Parent $OutputPath
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }

    $stream = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try {
        $stream.Write($Bytes, [int]$Entry.Offset, [int]$Entry.Length)
    } finally {
        $stream.Dispose()
    }
}

function New-XfingPayloadRecord {
    param(
        [string]$Status,
        $BaseEntry,
        $PatchEntry,
        [string]$ZipPath
    )

    return [pscustomobject]@{
        status = $Status
        path = $ZipPath
        name = $PatchEntry.Name
        rawName = $PatchEntry.RawName
        baseIndex = if ($BaseEntry) { $BaseEntry.Index } else { $null }
        patchIndex = $PatchEntry.Index
        width = $PatchEntry.Width
        height = $PatchEntry.Height
        flags = $PatchEntry.Flags
        dflags = $PatchEntry.DFlags
        avgColor = $PatchEntry.AvgColor
        length = $PatchEntry.Length
        sha256 = $PatchEntry.Hash
        baseSha256 = if ($BaseEntry) { $BaseEntry.Hash } else { $null }
    }
}

function Compare-XfingByteRanges {
    param(
        [byte[]]$BaseBytes,
        [byte[]]$PatchBytes,
        [int]$Length
    )

    $ranges = @()
    $start = -1
    $end = -1
    for ($index = 0; $index -lt $Length; $index++) {
        if ($BaseBytes[$index] -ne $PatchBytes[$index]) {
            if ($start -lt 0) {
                $start = $index
            }
            $end = $index
        } elseif ($start -ge 0) {
            $ranges += New-XfingByteRangeRecord -BaseBytes $BaseBytes -PatchBytes $PatchBytes -Start $start -End $end
            $start = -1
        }
    }
    if ($start -ge 0) {
        $ranges += New-XfingByteRangeRecord -BaseBytes $BaseBytes -PatchBytes $PatchBytes -Start $start -End $end
    }
    return @($ranges)
}

function New-XfingByteRangeRecord {
    param(
        [byte[]]$BaseBytes,
        [byte[]]$PatchBytes,
        [int]$Start,
        [int]$End
    )

    $length = $End - $Start + 1
    return [pscustomobject]@{
        offset = $Start
        length = $length
        baseHex = [BitConverter]::ToString($BaseBytes, $Start, $length).Replace("-", "").ToLowerInvariant()
        patchHex = [BitConverter]::ToString($PatchBytes, $Start, $length).Replace("-", "").ToLowerInvariant()
    }
}

function Read-XfingInt16Value {
    param([byte[]]$Bytes, [ref]$Position)

    $value = [BitConverter]::ToInt16($Bytes, $Position.Value)
    $Position.Value += 2
    return $value
}

function Read-XfingInt32Value {
    param([byte[]]$Bytes, [ref]$Position)

    $value = [BitConverter]::ToInt32($Bytes, $Position.Value)
    $Position.Value += 4
    return $value
}

function Read-XfingName13 {
    param([byte[]]$Bytes, [ref]$Position)

    $text = [System.Text.Encoding]::ASCII.GetString($Bytes, $Position.Value, 13)
    $Position.Value += 13
    return $text.Trim([char]0).TrimEnd()
}

function Read-XfingVClipRecord {
    param([byte[]]$Bytes, [ref]$Position)

    $playTime = Read-XfingInt32Value $Bytes $Position
    $numFrames = Read-XfingInt32Value $Bytes $Position
    $frameTime = Read-XfingInt32Value $Bytes $Position
    $flags = Read-XfingInt32Value $Bytes $Position
    $sound = Read-XfingInt16Value $Bytes $Position
    $frames = @()
    for ($frameIndex = 0; $frameIndex -lt 30; $frameIndex++) {
        $frames += Read-XfingInt16Value $Bytes $Position
    }
    $light = Read-XfingInt32Value $Bytes $Position
    return [pscustomobject]@{
        PlayTime = $playTime
        NumFrames = $numFrames
        FrameTime = $frameTime
        Flags = $flags
        Sound = $sound
        Frames = @($frames)
        Light = $light
    }
}

function Read-XfingHamSections {
    param([string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $position = [ref]0
    $hamId = Read-XfingInt32Value $bytes $position
    $version = Read-XfingInt32Value $bytes $position
    $textureCount = Read-XfingInt32Value $bytes $position
    if ($hamId -ne 558711112 -or $version -ne 3) {
        throw "Not a supported HAM: $Path id=$hamId version=$version"
    }

    $textureIndices = @()
    for ($textureIndex = 0; $textureIndex -lt $textureCount; $textureIndex++) {
        $textureIndices += Read-XfingInt16Value $bytes $position
    }

    $textures = @()
    for ($textureIndex = 0; $textureIndex -lt $textureCount; $textureIndex++) {
        $flags = $bytes[$position.Value]
        $pad0 = $bytes[$position.Value + 1]
        $pad1 = $bytes[$position.Value + 2]
        $pad2 = $bytes[$position.Value + 3]
        $position.Value += 4
        $lighting = Read-XfingInt32Value $bytes $position
        $damage = Read-XfingInt32Value $bytes $position
        $eclip = Read-XfingInt16Value $bytes $position
        $destroyed = Read-XfingInt16Value $bytes $position
        $slideU = Read-XfingInt16Value $bytes $position
        $slideV = Read-XfingInt16Value $bytes $position
        $textures += [pscustomobject]@{
            Index = $textureIndex
            Bitmap = $textureIndices[$textureIndex]
            Flags = $flags
            Pad0 = $pad0
            Pad1 = $pad1
            Pad2 = $pad2
            Lighting = $lighting
            Damage = $damage
            Eclip = $eclip
            Destroyed = $destroyed
            SlideU = $slideU
            SlideV = $slideV
        }
    }

    $soundCount = Read-XfingInt32Value $bytes $position
    $position.Value += $soundCount * 2

    $vclipCount = Read-XfingInt32Value $bytes $position
    $vclips = @()
    for ($sectionIndex = 0; $sectionIndex -lt $vclipCount; $sectionIndex++) {
        $record = Read-XfingVClipRecord $bytes $position
        $record | Add-Member -NotePropertyName Index -NotePropertyValue $sectionIndex
        $vclips += $record
    }

    $eclipCount = Read-XfingInt32Value $bytes $position
    $eclips = @()
    for ($sectionIndex = 0; $sectionIndex -lt $eclipCount; $sectionIndex++) {
        $vclip = Read-XfingVClipRecord $bytes $position
        $timeLeft = Read-XfingInt32Value $bytes $position
        $frameCount = Read-XfingInt32Value $bytes $position
        $changingWall = Read-XfingInt16Value $bytes $position
        $changingObject = Read-XfingInt16Value $bytes $position
        $flags = Read-XfingInt32Value $bytes $position
        $critClip = Read-XfingInt32Value $bytes $position
        $destBm = Read-XfingInt32Value $bytes $position
        $destVclip = Read-XfingInt32Value $bytes $position
        $destEclip = Read-XfingInt32Value $bytes $position
        $destSize = Read-XfingInt32Value $bytes $position
        $sound = Read-XfingInt32Value $bytes $position
        $seg = Read-XfingInt32Value $bytes $position
        $side = Read-XfingInt32Value $bytes $position
        $eclips += [pscustomobject]@{
            Index = $sectionIndex
            PlayTime = $vclip.PlayTime
            NumFrames = $vclip.NumFrames
            FrameTime = $vclip.FrameTime
            VclipFlags = $vclip.Flags
            VclipSound = $vclip.Sound
            Frames = @($vclip.Frames)
            Light = $vclip.Light
            TimeLeft = $timeLeft
            FrameCount = $frameCount
            ChangingWall = $changingWall
            ChangingObject = $changingObject
            Flags = $flags
            CritClip = $critClip
            DestBm = $destBm
            DestVclip = $destVclip
            DestEclip = $destEclip
            DestSize = $destSize
            Sound = $sound
            Seg = $seg
            Side = $side
        }
    }

    $wallCount = Read-XfingInt32Value $bytes $position
    $walls = @()
    for ($sectionIndex = 0; $sectionIndex -lt $wallCount; $sectionIndex++) {
        $playTime = Read-XfingInt32Value $bytes $position
        $numFrames = Read-XfingInt16Value $bytes $position
        $frames = @()
        for ($frameIndex = 0; $frameIndex -lt 50; $frameIndex++) {
            $frames += Read-XfingInt16Value $bytes $position
        }
        $openSound = Read-XfingInt16Value $bytes $position
        $closeSound = Read-XfingInt16Value $bytes $position
        $flags = Read-XfingInt16Value $bytes $position
        $filename = Read-XfingName13 $bytes $position
        $pad = $bytes[$position.Value]
        $position.Value += 1
        $walls += [pscustomobject]@{
            Index = $sectionIndex
            PlayTime = $playTime
            NumFrames = $numFrames
            Frames = @($frames)
            OpenSound = $openSound
            CloseSound = $closeSound
            Flags = $flags
            Filename = $filename
            Pad = $pad
        }
    }

    return [pscustomobject]@{
        Path = $Path
        HamId = $hamId
        Version = $version
        TextureCount = $textureCount
        SoundCount = $soundCount
        VclipCount = $vclipCount
        EclipCount = $eclipCount
        WallCount = $wallCount
        Textures = @($textures)
        Vclips = @($vclips)
        Eclips = @($eclips)
        Walls = @($walls)
        NextOffset = $position.Value
        Sha256 = Get-XfingSha256ForBytes -Bytes $bytes
    }
}

function Compare-XfingRowsByJson {
    param(
        [object[]]$BaseRows,
        [object[]]$PatchRows,
        [string]$Section
    )

    $rows = @()
    $commonCount = [Math]::Min($BaseRows.Count, $PatchRows.Count)
    for ($index = 0; $index -lt $commonCount; $index++) {
        $baseText = $BaseRows[$index] | ConvertTo-Json -Compress -Depth 20
        $patchText = $PatchRows[$index] | ConvertTo-Json -Compress -Depth 20
        if ($baseText -ne $patchText) {
            $rows += [pscustomobject]@{
                section = $Section
                status = "Changed"
                index = $index
                base = $BaseRows[$index]
                patch = $PatchRows[$index]
            }
        }
    }
    for ($index = $commonCount; $index -lt $PatchRows.Count; $index++) {
        $rows += [pscustomobject]@{
            section = $Section
            status = "Extra"
            index = $index
            base = $null
            patch = $PatchRows[$index]
        }
    }
    for ($index = $commonCount; $index -lt $BaseRows.Count; $index++) {
        $rows += [pscustomobject]@{
            section = $Section
            status = "Missing"
            index = $index
            base = $BaseRows[$index]
            patch = $null
        }
    }
    return @($rows)
}

function ConvertTo-XfingJsonPointerSegment {
    param([string]$Text)

    return $Text.Replace("~", "~0").Replace("/", "~1")
}

function New-XfingJsonPatchOperation {
    param(
        [ValidateSet("add", "remove", "replace", "test")]
        [string]$Op,
        [string]$Path,
        $Value = $null
    )

    $operation = [ordered]@{
        op = $Op
        path = $Path
    }
    if ($Op -ne "remove") {
        $operation.value = $Value
    }
    return [pscustomobject]$operation
}

function Convert-XfingRowsToJsonPatch {
    param(
        [object[]]$Rows,
        [string]$BasePath
    )

    $ops = @()
    foreach ($row in $Rows) {
        $path = "$BasePath/$(ConvertTo-XfingJsonPointerSegment ([string]$row.index))"
        if ($row.status -eq "Changed") {
            $ops += New-XfingJsonPatchOperation -Op "test" -Path $path -Value $row.base
            $ops += New-XfingJsonPatchOperation -Op "replace" -Path $path -Value $row.patch
        } elseif ($row.status -eq "Extra") {
            $ops += New-XfingJsonPatchOperation -Op "add" -Path $path -Value $row.patch
        } elseif ($row.status -eq "Missing") {
            $ops += New-XfingJsonPatchOperation -Op "test" -Path $path -Value $row.base
            $ops += New-XfingJsonPatchOperation -Op "remove" -Path $path
        }
    }
    return @($ops)
}

function Convert-XfingD1SurfaceRowsToJsonPatch {
    param([object[]]$Rows)

    $ops = @()
    foreach ($row in $Rows) {
        $level = ConvertTo-XfingJsonPointerSegment $row.level
        $surfaceKey = ConvertTo-XfingJsonPointerSegment ("{0}:{1}" -f $row.segment, $row.side)
        $path = "/levels/$level/surfaces/$surfaceKey"
        if ($row.status -eq "Changed") {
            $ops += New-XfingJsonPatchOperation -Op "test" -Path $path -Value $row.base
            $ops += New-XfingJsonPatchOperation -Op "replace" -Path $path -Value $row.patch
        } elseif ($row.status -eq "Extra") {
            $ops += New-XfingJsonPatchOperation -Op "add" -Path $path -Value $row.patch
        } elseif ($row.status -eq "Missing") {
            $ops += New-XfingJsonPatchOperation -Op "test" -Path $path -Value $row.base
            $ops += New-XfingJsonPatchOperation -Op "remove" -Path $path
        }
    }
    return @($ops)
}

function Export-XfingZipEntry {
    param(
        [string]$ZipPath,
        [string]$EntryName,
        [string]$OutputPath
    )

    $parent = Split-Path -Parent $OutputPath
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }

    $archive = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)
    try {
        $entry = $archive.GetEntry($EntryName)
        if (-not $entry) {
            throw "Missing $EntryName in $ZipPath"
        }
        $entryStream = $entry.Open()
        try {
            $output = [System.IO.File]::Open($OutputPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
            try {
                $entryStream.CopyTo($output)
            } finally {
                $output.Dispose()
            }
        } finally {
            $entryStream.Dispose()
        }
    } finally {
        $archive.Dispose()
    }
}

function Export-XfingHogEntry {
    param(
        [string]$HogPath,
        [string]$EntryName,
        [string]$OutputPath
    )

    $entryUpper = $EntryName.ToUpperInvariant()
    $stream = [System.IO.File]::OpenRead($HogPath)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        $signature = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(3))
        if ($signature -ne "DHF") {
            throw "Not a supported HOG: $HogPath"
        }
        while ($stream.Position -lt $stream.Length) {
            $name = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(13)).Trim([char]0).TrimEnd()
            $length = $reader.ReadInt32()
            if ($name.ToUpperInvariant() -eq $entryUpper) {
                $bytes = $reader.ReadBytes($length)
                $parent = Split-Path -Parent $OutputPath
                if (-not (Test-Path -LiteralPath $parent)) {
                    New-Item -ItemType Directory -Path $parent | Out-Null
                }
                [System.IO.File]::WriteAllBytes($OutputPath, $bytes)
                return
            }
            $stream.Position += $length
        }
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
    throw "Missing $EntryName in $HogPath"
}

function Read-XfingD1LevelSurfaces {
    param([string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        $null = $reader.ReadInt32()
        $version = $reader.ReadInt32()
        $mineOffset = $reader.ReadInt32()
        $null = $reader.ReadInt32()
        if ($version -lt 5) {
            $null = $reader.ReadInt32()
        }
        $stream.Position = $mineOffset
        $null = $reader.ReadByte()
        $vertexCount = $reader.ReadInt16()
        $segmentCount = $reader.ReadInt16()
        $null = $reader.ReadBytes($vertexCount * 12)
        $rows = @()
        for ($segment = 0; $segment -lt $segmentCount; $segment++) {
            $childMask = $reader.ReadByte()
            $children = @()
            for ($side = 0; $side -lt 6; $side++) {
                if (($childMask -band (1 -shl $side)) -ne 0) {
                    $children += $reader.ReadInt16()
                } else {
                    $children += -1
                }
            }
            for ($index = 0; $index -lt 8; $index++) {
                $null = $reader.ReadInt16()
            }
            if ($version -le 1 -and (($childMask -band (1 -shl 6)) -ne 0)) {
                $null = $reader.ReadBytes(4)
            }
            if ($version -le 5) {
                $null = $reader.ReadInt16()
            }
            $wallMask = $reader.ReadByte()
            $walls = @()
            for ($side = 0; $side -lt 6; $side++) {
                if (($wallMask -band (1 -shl $side)) -ne 0) {
                    $wall = $reader.ReadByte()
                    $walls += if ($wall -eq 255) { -1 } else { $wall }
                } else {
                    $walls += -1
                }
            }
            for ($side = 0; $side -lt 6; $side++) {
                if ($children[$side] -eq -1 -or $walls[$side] -ne -1) {
                    $raw1 = $reader.ReadUInt16()
                    $hasTmap2 = ($raw1 -band 0x8000) -ne 0
                    $raw2 = 0
                    if ($hasTmap2) {
                        $raw2 = $reader.ReadUInt16()
                    }
                    $uvls = @()
                    for ($uvlIndex = 0; $uvlIndex -lt 4; $uvlIndex++) {
                        $uvls += [pscustomobject]@{
                            u = $reader.ReadInt16()
                            v = $reader.ReadInt16()
                            l = $reader.ReadUInt16()
                        }
                    }
                    $rows += [pscustomobject]@{
                        segment = $segment
                        side = $side
                        child = $children[$side]
                        wall = $walls[$side]
                        rawTmap1 = $raw1
                        tmap1 = $raw1 -band 0x7fff
                        hasTmap2 = $hasTmap2
                        rawTmap2 = $raw2
                        tmap2 = $raw2 -band 0x3fff
                        orient = $raw2 -band 0xc000
                        uvls = @($uvls)
                    }
                }
            }
        }
        return [pscustomobject]@{
            path = $Path
            version = $version
            vertexCount = $vertexCount
            segmentCount = $segmentCount
            surfaces = @($rows)
        }
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

function Compare-XfingD1LevelSurfaces {
    param(
        $BaseLevel,
        $PatchLevel,
        [string]$LevelName
    )

    $baseMap = @{}
    foreach ($surface in $BaseLevel.surfaces) {
        $baseMap["$($surface.segment):$($surface.side)"] = $surface
    }
    $patchMap = @{}
    foreach ($surface in $PatchLevel.surfaces) {
        $patchMap["$($surface.segment):$($surface.side)"] = $surface
    }

    $rows = @()
    foreach ($key in ($baseMap.Keys + $patchMap.Keys | Sort-Object -Unique)) {
        $base = if ($baseMap.ContainsKey($key)) { $baseMap[$key] } else { $null }
        $patch = if ($patchMap.ContainsKey($key)) { $patchMap[$key] } else { $null }
        $status = "Same"
        if ($null -eq $base) {
            $status = "Extra"
        } elseif ($null -eq $patch) {
            $status = "Missing"
        } elseif (($base | ConvertTo-Json -Compress -Depth 20) -ne ($patch | ConvertTo-Json -Compress -Depth 20)) {
            $status = "Changed"
        }
        if ($status -ne "Same") {
            $rows += [pscustomobject]@{
                level = $LevelName
                status = $status
                segment = if ($patch) { $patch.segment } else { $base.segment }
                side = if ($patch) { $patch.side } else { $base.side }
                base = $base
                patch = $patch
            }
        }
    }
    return @($rows)
}

function Write-XfingJsonFile {
    param(
        [string]$Path,
        $Value,
        [int]$Depth = 40
    )

    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    $Value | ConvertTo-Json -Depth $Depth | Set-Content -LiteralPath $Path -Encoding utf8
}

function New-XfingDxaFromDirectory {
    param(
        [string]$SourceDir,
        [string]$OutputPath
    )

    if (Test-Path -LiteralPath $OutputPath) {
        Remove-Item -LiteralPath $OutputPath -Force
    }
    $archive = [System.IO.Compression.ZipFile]::Open($OutputPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        $files = Get-ChildItem -LiteralPath $SourceDir -Recurse -File | Sort-Object FullName
        foreach ($file in $files) {
            $relative = [System.IO.Path]::GetRelativePath($SourceDir, $file.FullName).Replace('\', '/')
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $archive,
                $file.FullName,
                $relative,
                [System.IO.Compression.CompressionLevel]::Optimal
            ) | Out-Null
        }
    } finally {
        $archive.Dispose()
    }
}