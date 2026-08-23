#!/usr/bin/env pwsh
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot '../../../android/helpers/powershell_compat.ps1')

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (-not ("DxxRedux.XfingValidation" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;

namespace DxxRedux
{
    public static class XfingValidation
    {
        private static void ValidateRleRow(byte[] bytes, int offset, int length, int width)
        {
            int source = offset;
            int sourceEnd = checked(offset + length);
            int decoded = 0;
            bool terminated = false;
            while (source < sourceEnd)
            {
                byte data = bytes[source++];
                if ((data & 0xe0) != 0xe0)
                {
                    decoded++;
                }
                else
                {
                    int count = data & 31;
                    if (count == 0)
                    {
                        terminated = true;
                        break;
                    }
                    if (source >= sourceEnd)
                        throw new InvalidDataException("RLE row ends inside a run");
                    source++;
                    decoded = checked(decoded + count);
                }
                if (decoded > width)
                    throw new InvalidDataException("RLE row exceeds its declared width");
            }
            if (!terminated || decoded != width || source != sourceEnd)
                throw new InvalidDataException("RLE row does not consume its declared input and width exactly");
        }

        public static void ValidateRleBitmap(byte[] bytes, int offset, int length, int width, int height, int rowSizeBytes)
        {
            int tableLength = checked(4 + checked(height * rowSizeBytes));
            if (offset < 0 || length < tableLength || offset > bytes.Length - length)
                throw new InvalidDataException("RLE header is outside its entry");
            int declaredLength = BitConverter.ToInt32(bytes, offset);
            if (declaredLength != length)
                throw new InvalidDataException("RLE declared length does not match its entry");
            int source = checked(offset + tableLength);
            int entryEnd = checked(offset + length);
            for (int row = 0; row < height; ++row)
            {
                int rowOffset = checked(offset + 4 + checked(row * rowSizeBytes));
                int rowSize = rowSizeBytes == 2 ? BitConverter.ToUInt16(bytes, rowOffset) : bytes[rowOffset];
                if (rowSize <= 0 || source > entryEnd - rowSize)
                    throw new InvalidDataException("RLE row is outside its entry");
                ValidateRleRow(bytes, source, rowSize, width);
                source += rowSize;
            }
            if (source != entryEnd)
                throw new InvalidDataException("RLE rows do not consume their entry exactly");
        }
    }
}
'@
}

if (-not ("DxxRedux.XfingPngEncoder" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.IO.Compression;
using System.Text;

namespace DxxRedux
{
    public static class XfingPngEncoder
    {
        private static readonly byte[] Signature = { 137, 80, 78, 71, 13, 10, 26, 10 };
        private static readonly uint[] CrcTable = CreateCrcTable();

        private static uint[] CreateCrcTable()
        {
            uint[] table = new uint[256];
            for (uint index = 0; index < table.Length; ++index)
            {
                uint value = index;
                for (int bit = 0; bit < 8; ++bit)
                    value = (value & 1) != 0 ? 0xedb88320U ^ (value >> 1) : value >> 1;
                table[index] = value;
            }
            return table;
        }

        private static void WriteBigEndian(Stream stream, uint value)
        {
            stream.WriteByte((byte)(value >> 24));
            stream.WriteByte((byte)(value >> 16));
            stream.WriteByte((byte)(value >> 8));
            stream.WriteByte((byte)value);
        }

        private static void WriteChunk(Stream stream, string name, byte[] data)
        {
            byte[] nameBytes = Encoding.ASCII.GetBytes(name);
            WriteBigEndian(stream, checked((uint)data.Length));
            stream.Write(nameBytes, 0, nameBytes.Length);
            stream.Write(data, 0, data.Length);

            uint crc = 0xffffffffU;
            for (int index = 0; index < nameBytes.Length; ++index)
                crc = CrcTable[(crc ^ nameBytes[index]) & 0xff] ^ (crc >> 8);
            for (int index = 0; index < data.Length; ++index)
                crc = CrcTable[(crc ^ data[index]) & 0xff] ^ (crc >> 8);
            WriteBigEndian(stream, crc ^ 0xffffffffU);
        }

        public static void Preflight()
        {
            using (MemoryStream compressed = new MemoryStream())
            using (ZLibStream zlib = new ZLibStream(compressed, CompressionLevel.Optimal, true))
                zlib.WriteByte(0);
        }

        public static void WriteRgba(string path, int width, int height, byte[] rgba)
        {
            if (width <= 0 || height <= 0)
                throw new ArgumentOutOfRangeException("PNG dimensions must be positive");
            int stride = checked(width * 4);
            if (rgba == null || rgba.Length != checked(stride * height))
                throw new ArgumentException("RGBA input length does not match PNG dimensions", "rgba");

            byte[] header = new byte[13];
            header[0] = (byte)(width >> 24);
            header[1] = (byte)(width >> 16);
            header[2] = (byte)(width >> 8);
            header[3] = (byte)width;
            header[4] = (byte)(height >> 24);
            header[5] = (byte)(height >> 16);
            header[6] = (byte)(height >> 8);
            header[7] = (byte)height;
            header[8] = 8;
            header[9] = 6;

            using (MemoryStream compressed = new MemoryStream())
            {
                using (ZLibStream zlib = new ZLibStream(compressed, CompressionLevel.Optimal, true))
                {
                    for (int row = 0; row < height; ++row)
                    {
                        zlib.WriteByte(0);
                        zlib.Write(rgba, checked(row * stride), stride);
                    }
                }

                using (FileStream output = new FileStream(path, FileMode.Create, FileAccess.Write, FileShare.None))
                {
                    output.Write(Signature, 0, Signature.Length);
                    WriteChunk(output, "IHDR", header);
                    WriteChunk(output, "IDAT", compressed.ToArray());
                    WriteChunk(output, "IEND", new byte[0]);
                }
            }
        }
    }
}
'@
}
[DxxRedux.XfingPngEncoder]::Preflight()

$script:XfingBitmapFlagTransparent = 1
$script:XfingBitmapFlagSuperTransparent = 2
$script:XfingBitmapFlagRle = 8
$script:XfingBitmapFlagRleBig = 32
$script:XfingMaxInputBytes = 268435456
$script:XfingMaxPigBitmaps = 2620
$script:XfingMaxPigSounds = 254
$script:XfingMaxBitmapDimension = 4096
$script:XfingMaxBitmapPixels = 16777216
$script:XfingMaxAggregatePixels = 67108864

function Assert-XfingSpan {
    param(
        [long]$Offset,
        [long]$Length,
        [long]$Limit,
        [string]$Description
    )

    if ($Offset -lt 0 -or $Length -lt 0 -or $Offset -gt $Limit -or $Length -gt ($Limit - $Offset)) {
        throw "$Description is outside its bounded input: offset=$Offset length=$Length limit=$Limit"
    }
}

function Assert-XfingCount {
    param(
        [long]$Count,
        [long]$Maximum,
        [string]$Description
    )

    if ($Count -lt 0 -or $Count -gt $Maximum) {
        throw "$Description count $Count is outside 0..$Maximum"
    }
}

function Read-XfingBoundedFile {
    param(
        [string]$Path,
        [long]$MaximumBytes = $script:XfingMaxInputBytes
    )

    $length = (Get-Item -LiteralPath $Path).Length
    if ($length -gt $MaximumBytes) {
        throw "Input exceeds the $MaximumBytes byte limit: $Path size=$length"
    }
    return , ([System.IO.File]::ReadAllBytes($Path))
}

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

    if ($Length -lt 0 -or $Reader.BaseStream.Position -gt $Reader.BaseStream.Length - $Length) {
        throw "Truncated fixed ASCII field at offset $($Reader.BaseStream.Position), length $Length"
    }
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

    $bytes = Read-XfingBoundedFile -Path $Path -MaximumBytes 65536
    if ($bytes.Length -lt 768) {
        throw "Palette file is shorter than 768 bytes: $Path size=$($bytes.Length)"
    }
    $palette = [byte[]]::new(768)
    [Array]::Copy($bytes, 0, $palette, 0, 768)
    return , $palette
}

function Convert-XfingPaletteComponent {
    param([byte]$Value)

    return [byte]([Math]::Min(255, [int]$Value * 4))
}

function Expand-XfingRleRow {
    param(
        [byte[]]$Bytes,
        [int]$SourceOffset,
        [int]$SourceLength,
        [byte[]]$Destination,
        [int]$DestinationOffset,
        [int]$Width
    )

    Assert-XfingSpan -Offset $SourceOffset -Length $SourceLength -Limit $Bytes.Length -Description "RLE row"
    $source = $SourceOffset
    $sourceEnd = $SourceOffset + $SourceLength
    $dest = $DestinationOffset
    $destEnd = $DestinationOffset + $Width
    $terminated = $false
    while ($source -lt $sourceEnd) {
        $data = $Bytes[$source]
        $source++
        if (($data -band 0xE0) -ne 0xE0) {
            $Destination[$dest] = $data
            $dest++
        } else {
            $count = $data -band 31
            if ($count -eq 0) {
                $terminated = $true
                break
            }
            if ($source -ge $sourceEnd) {
                throw "RLE row ends inside a run"
            }
            $color = $Bytes[$source]
            $source++
            if ($count -gt ($destEnd - $dest)) {
                throw "RLE row run exceeds its declared width $Width"
            }
            for ($index = 0; $index -lt $count; $index++) {
                $Destination[$dest] = $color
                $dest++
            }
        }
    }
    if (-not $terminated -or $dest -ne $destEnd -or $source -ne $sourceEnd) {
        throw "RLE row decoded to $($dest - $DestinationOffset) pixels, expected $Width"
    }
}

function Expand-XfingBitmapPixels {
    param(
        $Pig,
        $Entry
    )

    $size = [long]$Entry.Width * [long]$Entry.Height
    if ($size -le 0 -or $size -gt $script:XfingMaxBitmapPixels) {
        throw "Bitmap $($Entry.Name) pixel count $size is outside the supported range"
    }
    $pixels = [byte[]]::new($size)
    if (($Entry.Flags -band $script:XfingBitmapFlagRle) -ne 0) {
        $rowSizeBytes = if (($Entry.Flags -band $script:XfingBitmapFlagRleBig) -ne 0) { 2 } else { 1 }
        $tableLength = 4 + ([long]$Entry.Height * $rowSizeBytes)
        Assert-XfingSpan -Offset $Entry.Offset -Length $tableLength -Limit ($Entry.Offset + $Entry.Length) -Description "RLE header for $($Entry.Name)"
        $declaredLength = [BitConverter]::ToInt32($Pig.Bytes, [int]$Entry.Offset)
        if ($declaredLength -ne $Entry.Length) {
            throw "RLE bitmap $($Entry.Name) declares $declaredLength bytes, expected $($Entry.Length)"
        }
        $dataOffset = [int]($Entry.Offset + $tableLength)
        $sourceOffset = $dataOffset
        for ($row = 0; $row -lt [int]$Entry.Height; $row++) {
            $rowSizeOffset = [int]$Entry.Offset + 4 + ($row * $rowSizeBytes)
            $rowSize = if ($rowSizeBytes -eq 2) {
                [BitConverter]::ToUInt16($Pig.Bytes, $rowSizeOffset)
            } else {
                [int]$Pig.Bytes[$rowSizeOffset]
            }
            if ($rowSize -le 0) {
                throw "RLE bitmap $($Entry.Name) row $row has invalid size $rowSize"
            }
            Assert-XfingSpan -Offset $sourceOffset -Length $rowSize -Limit ($Entry.Offset + $Entry.Length) -Description "RLE bitmap $($Entry.Name) row $row"
            Expand-XfingRleRow `
                -Bytes $Pig.Bytes `
                -SourceOffset $sourceOffset `
                -SourceLength $rowSize `
                -Destination $pixels `
                -DestinationOffset ($row * [int]$Entry.Width) `
                -Width ([int]$Entry.Width)
            $sourceOffset += $rowSize
        }
        if ($sourceOffset -ne ($Entry.Offset + $Entry.Length)) {
            throw "RLE bitmap $($Entry.Name) row sizes do not consume its entry"
        }
    } else {
        if ($Entry.Length -ne $size) {
            throw "Bitmap $($Entry.Name) has $($Entry.Length) bytes, expected $size"
        }
        [Array]::Copy($Pig.Bytes, [int]$Entry.Offset, $pixels, 0, $size)
    }
    return , $pixels
}

function Test-XfingBitmapPayload {
    param(
        $Pig,
        $Entry
    )

    $pixelCount = [long]$Entry.Width * [long]$Entry.Height
    if ($pixelCount -le 0 -or $pixelCount -gt $script:XfingMaxBitmapPixels) {
        throw "Bitmap $($Entry.Name) pixel count $pixelCount is outside the supported range"
    }
    if (($Entry.Flags -band $script:XfingBitmapFlagRle) -eq 0) {
        if ($Entry.Length -ne $pixelCount) {
            throw "Bitmap $($Entry.Name) has $($Entry.Length) bytes, expected $pixelCount"
        }
        return
    }

    $rowSizeBytes = if (($Entry.Flags -band $script:XfingBitmapFlagRleBig) -ne 0) { 2 } else { 1 }
    try {
        [DxxRedux.XfingValidation]::ValidateRleBitmap(
            $Pig.Bytes,
            [int]$Entry.Offset,
            [int]$Entry.Length,
            [int]$Entry.Width,
            [int]$Entry.Height,
            $rowSizeBytes)
    } catch {
        $reason = if ($_.Exception.InnerException) { $_.Exception.InnerException.Message } else { $_.Exception.Message }
        throw "RLE bitmap $($Entry.Name) is invalid: $reason"
    }
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

    [DxxRedux.XfingPngEncoder]::WriteRgba($Path, $Width, $Height, $Rgba)
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

function Read-XfingD1Pig {
    param([string]$Path)

    $bytes = Read-XfingBoundedFile $Path
    Assert-XfingSpan -Offset 0 -Length 8 -Limit $bytes.Length -Description "D1 PIG header"
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

    Assert-XfingSpan -Offset $pigDataStart -Length 8 -Limit $fileLength -Description "D1 PIG data header"
    $stream.Position = $pigDataStart
    $bitmapCount = $reader.ReadInt32()
    $soundCount = $reader.ReadInt32()
    Assert-XfingCount -Count $bitmapCount -Maximum $script:XfingMaxPigBitmaps -Description "D1 PIG bitmap"
    Assert-XfingCount -Count $soundCount -Maximum $script:XfingMaxPigSounds -Description "D1 PIG sound"
    $headerLength = 8L + ([long]$bitmapCount * 17L) + ([long]$soundCount * 20L)
    Assert-XfingSpan -Offset $pigDataStart -Length $headerLength -Limit $fileLength -Description "D1 PIG tables"
    $bitmapHeaders = [System.Collections.Generic.List[object]]::new($bitmapCount)
    for ($entryIndex = 0; $entryIndex -lt $bitmapCount; $entryIndex++) {
        $rawName = Read-XfingFixedAscii -Reader $reader -Length 8
        $dflags = $reader.ReadByte()
        $widthByte = $reader.ReadByte()
        $heightByte = $reader.ReadByte()
        $flags = $reader.ReadByte()
        $avgColor = $reader.ReadByte()
        $relativeOffset = $reader.ReadInt32()
        $width = if (($dflags -band 128) -ne 0) { $widthByte + 256 } else { $widthByte }
        if ($width -le 0 -or $heightByte -le 0) {
            throw "D1 PIG bitmap $entryIndex has invalid dimensions ${width}x$heightByte"
        }
        $bitmapHeaders.Add([pscustomobject]@{
                Index = $entryIndex + 1
                Name = Get-XfingBitmapName -Name $rawName -DFlags $dflags
                RawName = $rawName
                DFlags = $dflags
                Width = $width
                Height = $heightByte
                Flags = $flags
                AvgColor = $avgColor
                RelativeOffset = $relativeOffset
            })
    }

    $soundHeaders = [System.Collections.Generic.List[object]]::new($soundCount)
    for ($entryIndex = 0; $entryIndex -lt $soundCount; $entryIndex++) {
        $soundName = Read-XfingFixedAscii -Reader $reader -Length 8
        $length = $reader.ReadInt32()
        $dataLength = $reader.ReadInt32()
        $relativeOffset = $reader.ReadInt32()
        if ($length -lt 0 -or $dataLength -lt 0 -or $relativeOffset -lt 0) {
            throw "D1 PIG sound $entryIndex has a negative length or offset"
        }
        $soundHeaders.Add([pscustomobject]@{
                Name = $soundName
                Length = $length
                DataLength = $dataLength
                RelativeOffset = $relativeOffset
            })
    }

    $dataBase = [long]$pigDataStart + $headerLength
    $allHeaders = @($bitmapHeaders) + @($soundHeaders)
    $cutpoints = [System.Collections.Generic.List[int]]::new($allHeaders.Count)
    $previousOffset = -1L
    foreach ($header in $allHeaders) {
        $absoluteOffset = $dataBase + [long]$header.RelativeOffset
        if ($header.RelativeOffset -lt 0 -or $absoluteOffset -le $previousOffset) {
            throw "D1 PIG entry offsets are negative, duplicate, or unordered"
        }
        Assert-XfingSpan -Offset $absoluteOffset -Length 1 -Limit $fileLength -Description "D1 PIG entry payload"
        $cutpoints.Add([int]$absoluteOffset)
        $previousOffset = $absoluteOffset
    }
    $nextByOffset = @{}
    for ($index = 0; $index -lt $cutpoints.Count; $index++) {
        $nextByOffset[$cutpoints[$index]] = if ($index + 1 -lt $cutpoints.Count) { $cutpoints[$index + 1] } else { $fileLength }
    }

    $entries = [System.Collections.Generic.List[object]]::new($bitmapCount)
    $aggregatePixels = 0L
    foreach ($header in $bitmapHeaders) {
        $absoluteOffset = $dataBase + $header.RelativeOffset
        $nextOffset = $nextByOffset[[int]$absoluteOffset]
        $payloadLength = $nextOffset - $absoluteOffset
        Assert-XfingSpan -Offset $absoluteOffset -Length $payloadLength -Limit $fileLength -Description "D1 PIG bitmap $($header.Name)"
        $pixels = [long]$header.Width * [long]$header.Height
        if ($pixels -le 0 -or $pixels -gt $script:XfingMaxBitmapPixels) {
            throw "D1 PIG bitmap $($header.Name) has unsupported pixel count $pixels"
        }
        $aggregatePixels += $pixels
        if ($aggregatePixels -gt $script:XfingMaxAggregatePixels) {
            throw "D1 PIG exceeds the aggregate pixel budget"
        }
        $entries.Add([pscustomobject]@{
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
            })
    }
    foreach ($header in $soundHeaders) {
        $absoluteOffset = $dataBase + $header.RelativeOffset
        $nextOffset = $nextByOffset[[int]$absoluteOffset]
        $payloadLength = $nextOffset - $absoluteOffset
        if ($header.DataLength -gt $payloadLength) {
            throw "D1 PIG sound $($header.Name) needs $($header.DataLength) bytes, entry has $payloadLength"
        }
    }

    $reader.Dispose()
    $stream.Dispose()
    $pig = [pscustomobject]@{
        Path = $Path
        Format = "d1pig"
        PigDataStart = $pigDataStart
        Bytes = $bytes
        BitmapCount = $bitmapCount
        SoundCount = $soundCount
        Entries = @($entries)
        Sha256 = Get-XfingSha256ForBytes -Bytes $bytes
    }
    foreach ($entry in $pig.Entries) {
        Test-XfingBitmapPayload -Pig $pig -Entry $entry
    }
    return $pig
}

function Read-XfingD2Pig {
    param([string]$Path)

    $bytes = Read-XfingBoundedFile $Path
    Assert-XfingSpan -Offset 0 -Length 12 -Limit $bytes.Length -Description "D2 PIG header"
    $stream = [System.IO.MemoryStream]::new($bytes)
    $reader = [System.IO.BinaryReader]::new($stream)
    $magic = $reader.ReadInt32()
    $version = $reader.ReadInt32()
    if ($magic -ne 1195987024 -or $version -ne 2) {
        throw "Not a supported D2 PIG: $Path magic=$magic version=$version"
    }
    $bitmapCount = $reader.ReadInt32()
    Assert-XfingCount -Count $bitmapCount -Maximum $script:XfingMaxPigBitmaps -Description "D2 PIG bitmap"
    $headerLength = 12L + ([long]$bitmapCount * 18L)
    Assert-XfingSpan -Offset 0 -Length $headerLength -Limit $bytes.Length -Description "D2 PIG table"
    $headers = [System.Collections.Generic.List[object]]::new($bitmapCount)
    for ($entryIndex = 0; $entryIndex -lt $bitmapCount; $entryIndex++) {
        $rawName = Read-XfingFixedAscii -Reader $reader -Length 8
        $dflags = $reader.ReadByte()
        $widthByte = $reader.ReadByte()
        $heightByte = $reader.ReadByte()
        $whExtra = $reader.ReadByte()
        $flags = $reader.ReadByte()
        $avgColor = $reader.ReadByte()
        $relativeOffset = $reader.ReadInt32()
        $width = $widthByte + (($whExtra -band 0x0f) -shl 8)
        $height = $heightByte + (($whExtra -band 0xf0) -shl 4)
        if ($width -le 0 -or $height -le 0 -or $width -gt $script:XfingMaxBitmapDimension -or $height -gt $script:XfingMaxBitmapDimension) {
            throw "D2 PIG bitmap $entryIndex has unsupported dimensions ${width}x$height"
        }
        $headers.Add([pscustomobject]@{
                Index = $entryIndex + 1
                Name = Get-XfingBitmapName -Name $rawName -DFlags $dflags
                RawName = $rawName
                DFlags = $dflags
                Width = $width
                Height = $height
                Flags = $flags
                AvgColor = $avgColor
                RelativeOffset = $relativeOffset
            })
    }

    $dataBase = $headerLength
    $fileLength = $bytes.Length
    $cutpoints = [System.Collections.Generic.List[int]]::new($bitmapCount)
    $previousOffset = -1L
    foreach ($header in $headers) {
        $absoluteOffset = $dataBase + [long]$header.RelativeOffset
        if ($header.RelativeOffset -lt 0 -or $absoluteOffset -le $previousOffset) {
            throw "D2 PIG entry offsets are negative, duplicate, or unordered"
        }
        Assert-XfingSpan -Offset $absoluteOffset -Length 1 -Limit $fileLength -Description "D2 PIG entry payload"
        $cutpoints.Add([int]$absoluteOffset)
        $previousOffset = $absoluteOffset
    }
    $nextByOffset = @{}
    for ($index = 0; $index -lt $cutpoints.Count; $index++) {
        $nextByOffset[$cutpoints[$index]] = if ($index + 1 -lt $cutpoints.Count) { $cutpoints[$index + 1] } else { $fileLength }
    }
    $entries = [System.Collections.Generic.List[object]]::new($bitmapCount)
    $aggregatePixels = 0L
    foreach ($header in $headers) {
        $absoluteOffset = $dataBase + $header.RelativeOffset
        $nextOffset = $nextByOffset[[int]$absoluteOffset]
        $payloadLength = $nextOffset - $absoluteOffset
        Assert-XfingSpan -Offset $absoluteOffset -Length $payloadLength -Limit $fileLength -Description "D2 PIG bitmap $($header.Name)"
        $pixels = [long]$header.Width * [long]$header.Height
        if ($pixels -gt $script:XfingMaxBitmapPixels) {
            throw "D2 PIG bitmap $($header.Name) has unsupported pixel count $pixels"
        }
        $aggregatePixels += $pixels
        if ($aggregatePixels -gt $script:XfingMaxAggregatePixels) {
            throw "D2 PIG exceeds the aggregate pixel budget"
        }
        $entries.Add([pscustomobject]@{
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
            })
    }

    $reader.Dispose()
    $stream.Dispose()
    $pig = [pscustomobject]@{
        Path = $Path
        Format = "d2pig"
        Bytes = $bytes
        BitmapCount = $bitmapCount
        SoundCount = 0
        Entries = @($entries)
        Sha256 = Get-XfingSha256ForBytes -Bytes $bytes
    }
    foreach ($entry in $pig.Entries) {
        Test-XfingBitmapPayload -Pig $pig -Entry $entry
    }
    return $pig
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

    Assert-XfingSpan -Offset $Position.Value -Length 2 -Limit $Bytes.Length -Description "16-bit field"
    $value = [BitConverter]::ToInt16($Bytes, $Position.Value)
    $Position.Value += 2
    return $value
}

function Read-XfingInt32Value {
    param([byte[]]$Bytes, [ref]$Position)

    Assert-XfingSpan -Offset $Position.Value -Length 4 -Limit $Bytes.Length -Description "32-bit field"
    $value = [BitConverter]::ToInt32($Bytes, $Position.Value)
    $Position.Value += 4
    return $value
}

function Read-XfingName13 {
    param([byte[]]$Bytes, [ref]$Position)

    Assert-XfingSpan -Offset $Position.Value -Length 13 -Limit $Bytes.Length -Description "13-byte name"
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
    $frames = [System.Collections.Generic.List[object]]::new(30)
    for ($frameIndex = 0; $frameIndex -lt 30; $frameIndex++) {
        $frames.Add((Read-XfingInt16Value $Bytes $Position))
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

    $bytes = Read-XfingBoundedFile $Path
    Assert-XfingSpan -Offset 0 -Length 12 -Limit $bytes.Length -Description "HAM header"
    $position = [ref]0
    $hamId = Read-XfingInt32Value $bytes $position
    $version = Read-XfingInt32Value $bytes $position
    $textureCount = Read-XfingInt32Value $bytes $position
    if ($hamId -ne 558711112 -or $version -ne 3) {
        throw "Not a supported HAM: $Path id=$hamId version=$version"
    }
    Assert-XfingCount -Count $textureCount -Maximum 1200 -Description "HAM texture"
    Assert-XfingSpan -Offset $position.Value -Length ([long]$textureCount * 22L) -Limit $bytes.Length -Description "HAM texture sections"

    $textureIndices = [System.Collections.Generic.List[object]]::new($textureCount)
    for ($textureIndex = 0; $textureIndex -lt $textureCount; $textureIndex++) {
        $textureIndices.Add((Read-XfingInt16Value $bytes $position))
    }

    $textures = [System.Collections.Generic.List[object]]::new($textureCount)
    for ($textureIndex = 0; $textureIndex -lt $textureCount; $textureIndex++) {
        Assert-XfingSpan -Offset $position.Value -Length 20 -Limit $bytes.Length -Description "HAM texture $textureIndex"
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
        $textures.Add([pscustomobject]@{
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
            })
    }

    $soundCount = Read-XfingInt32Value $bytes $position
    Assert-XfingCount -Count $soundCount -Maximum 254 -Description "HAM sound"
    Assert-XfingSpan -Offset $position.Value -Length ([long]$soundCount * 2L) -Limit $bytes.Length -Description "HAM sound table"
    $position.Value += [int]($soundCount * 2)

    $vclipCount = Read-XfingInt32Value $bytes $position
    Assert-XfingCount -Count $vclipCount -Maximum 110 -Description "HAM vclip"
    Assert-XfingSpan -Offset $position.Value -Length ([long]$vclipCount * 82L) -Limit $bytes.Length -Description "HAM vclip section"
    $vclips = [System.Collections.Generic.List[object]]::new($vclipCount)
    for ($sectionIndex = 0; $sectionIndex -lt $vclipCount; $sectionIndex++) {
        $record = Read-XfingVClipRecord $bytes $position
        $record | Add-Member -NotePropertyName Index -NotePropertyValue $sectionIndex
        $vclips.Add($record)
    }

    $eclipCount = Read-XfingInt32Value $bytes $position
    Assert-XfingCount -Count $eclipCount -Maximum 110 -Description "HAM eclip"
    Assert-XfingSpan -Offset $position.Value -Length ([long]$eclipCount * 130L) -Limit $bytes.Length -Description "HAM eclip section"
    $eclips = [System.Collections.Generic.List[object]]::new($eclipCount)
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
        $eclips.Add([pscustomobject]@{
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
            })
    }

    $wallCount = Read-XfingInt32Value $bytes $position
    Assert-XfingCount -Count $wallCount -Maximum 60 -Description "HAM wall clip"
    Assert-XfingSpan -Offset $position.Value -Length ([long]$wallCount * 126L) -Limit $bytes.Length -Description "HAM wall clip section"
    $walls = [System.Collections.Generic.List[object]]::new($wallCount)
    for ($sectionIndex = 0; $sectionIndex -lt $wallCount; $sectionIndex++) {
        $playTime = Read-XfingInt32Value $bytes $position
        $numFrames = Read-XfingInt16Value $bytes $position
        $frames = [System.Collections.Generic.List[object]]::new(50)
        for ($frameIndex = 0; $frameIndex -lt 50; $frameIndex++) {
            $frames.Add((Read-XfingInt16Value $bytes $position))
        }
        $openSound = Read-XfingInt16Value $bytes $position
        $closeSound = Read-XfingInt16Value $bytes $position
        $flags = Read-XfingInt16Value $bytes $position
        $filename = Read-XfingName13 $bytes $position
        $pad = $bytes[$position.Value]
        $position.Value += 1
        Assert-XfingSpan -Offset $position.Value -Length 1 -Limit $bytes.Length -Description "HAM wall clip padding"
        $walls.Add([pscustomobject]@{
                Index = $sectionIndex
                PlayTime = $playTime
                NumFrames = $numFrames
                Frames = @($frames)
                OpenSound = $openSound
                CloseSound = $closeSound
                Flags = $flags
                Filename = $filename
                Pad = $pad
            })
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
    $hogLength = (Get-Item -LiteralPath $HogPath).Length
    if ($hogLength -gt $script:XfingMaxInputBytes) {
        throw "HOG exceeds the $($script:XfingMaxInputBytes) byte limit: $HogPath size=$hogLength"
    }
    $stream = [System.IO.File]::OpenRead($HogPath)
    $reader = [System.IO.BinaryReader]::new($stream)
    $selectedBytes = $null
    try {
        Assert-XfingSpan -Offset 0 -Length 3 -Limit $stream.Length -Description "HOG signature"
        $signature = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(3))
        if ($signature -ne "DHF") {
            throw "Not a supported HOG: $HogPath"
        }
        while ($stream.Position -lt $stream.Length) {
            Assert-XfingSpan -Offset $stream.Position -Length 17 -Limit $stream.Length -Description "HOG member header"
            $nameBytes = $reader.ReadBytes(13)
            $name = [System.Text.Encoding]::ASCII.GetString($nameBytes).Trim([char]0).TrimEnd()
            $length = $reader.ReadInt32()
            if ($length -le 0 -or $length -gt $script:XfingMaxInputBytes) {
                throw "HOG member $name has invalid length $length"
            }
            Assert-XfingSpan -Offset $stream.Position -Length $length -Limit $stream.Length -Description "HOG member $name"
            if ($name.ToUpperInvariant() -eq $entryUpper) {
                if ($null -ne $selectedBytes) {
                    throw "Duplicate $EntryName in $HogPath"
                }
                $selectedBytes = $reader.ReadBytes($length)
            } else {
                $stream.Position += $length
            }
        }
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
    if ($null -eq $selectedBytes) {
        throw "Missing $EntryName in $HogPath"
    }
    $parent = Split-Path -Parent $OutputPath
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    [System.IO.File]::WriteAllBytes($OutputPath, $selectedBytes)
}

function Read-XfingD1LevelSurfaces {
    param([string]$Path)

    $fileLength = (Get-Item -LiteralPath $Path).Length
    if ($fileLength -gt 33554432) {
        throw "D1 level exceeds the 33554432 byte limit: $Path size=$fileLength"
    }
    $stream = [System.IO.File]::OpenRead($Path)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        Assert-XfingSpan -Offset 0 -Length 16 -Limit $stream.Length -Description "D1 level header"
        $signature = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
        if ($signature -ne "LVLP") {
            throw "Not a supported D1 level: $Path signature=$signature"
        }
        $version = $reader.ReadInt32()
        $mineOffset = $reader.ReadInt32()
        $gameOffset = $reader.ReadInt32()
        if ($version -lt 1 -or $version -gt 7) {
            throw "Unsupported D1 level version $version in $Path"
        }
        $headerLength = if ($version -lt 5) { 20 } else { 16 }
        if ($version -lt 5) {
            Assert-XfingSpan -Offset $stream.Position -Length 4 -Limit $stream.Length -Description "D1 level hostage offset"
            $null = $reader.ReadInt32()
        }
        if ($mineOffset -lt $headerLength -or $mineOffset -ge $stream.Length) {
            throw "D1 level mine offset $mineOffset is outside the file"
        }
        if ($gameOffset -le $mineOffset -or $gameOffset -gt $stream.Length) {
            throw "D1 level game offset $gameOffset does not follow the compiled mine"
        }
        $mineEnd = [long]$gameOffset
        Assert-XfingSpan -Offset $mineOffset -Length ($mineEnd - $mineOffset) -Limit $stream.Length -Description "D1 compiled mine"
        $stream.Position = $mineOffset
        Assert-XfingSpan -Offset $stream.Position -Length 5 -Limit $mineEnd -Description "D1 compiled mine header"
        $null = $reader.ReadByte()
        $vertexCount = $reader.ReadInt16()
        $segmentCount = $reader.ReadInt16()
        Assert-XfingCount -Count $vertexCount -Maximum 32767 -Description "D1 level vertex"
        Assert-XfingCount -Count $segmentCount -Maximum 9000 -Description "D1 level segment"
        $vertexBytes = [long]$vertexCount * 12L
        Assert-XfingSpan -Offset $stream.Position -Length $vertexBytes -Limit $mineEnd -Description "D1 level vertices"
        $stream.Position += $vertexBytes
        $rows = [System.Collections.Generic.List[object]]::new([Math]::Min(54000, $segmentCount * 6))
        for ($segment = 0; $segment -lt $segmentCount; $segment++) {
            Assert-XfingSpan -Offset $stream.Position -Length 1 -Limit $mineEnd -Description "D1 segment $segment child mask"
            $childMask = $reader.ReadByte()
            $children = [System.Collections.Generic.List[object]]::new(6)
            for ($side = 0; $side -lt 6; $side++) {
                if (($childMask -band (1 -shl $side)) -ne 0) {
                    Assert-XfingSpan -Offset $stream.Position -Length 2 -Limit $mineEnd -Description "D1 segment $segment child"
                    $children.Add($reader.ReadInt16())
                } else {
                    $children.Add(-1)
                }
            }
            Assert-XfingSpan -Offset $stream.Position -Length 16 -Limit $mineEnd -Description "D1 segment $segment vertices"
            for ($index = 0; $index -lt 8; $index++) {
                $null = $reader.ReadInt16()
            }
            if ($version -le 1 -and (($childMask -band (1 -shl 6)) -ne 0)) {
                Assert-XfingSpan -Offset $stream.Position -Length 4 -Limit $mineEnd -Description "D1 segment $segment special data"
                $stream.Position += 4
            }
            if ($version -le 5) {
                Assert-XfingSpan -Offset $stream.Position -Length 2 -Limit $mineEnd -Description "D1 segment $segment static light"
                $null = $reader.ReadInt16()
            }
            Assert-XfingSpan -Offset $stream.Position -Length 1 -Limit $mineEnd -Description "D1 segment $segment wall mask"
            $wallMask = $reader.ReadByte()
            $walls = [System.Collections.Generic.List[object]]::new(6)
            for ($side = 0; $side -lt 6; $side++) {
                if (($wallMask -band (1 -shl $side)) -ne 0) {
                    Assert-XfingSpan -Offset $stream.Position -Length 1 -Limit $mineEnd -Description "D1 segment $segment wall"
                    $wall = $reader.ReadByte()
                    $walls.Add($(if ($wall -eq 255) { -1 } else { $wall }))
                } else {
                    $walls.Add(-1)
                }
            }
            for ($side = 0; $side -lt 6; $side++) {
                if ($children[$side] -eq -1 -or $walls[$side] -ne -1) {
                    Assert-XfingSpan -Offset $stream.Position -Length 2 -Limit $mineEnd -Description "D1 segment $segment side $side texture"
                    $raw1 = $reader.ReadUInt16()
                    $hasTmap2 = ($raw1 -band 0x8000) -ne 0
                    $raw2 = 0
                    if ($hasTmap2) {
                        Assert-XfingSpan -Offset $stream.Position -Length 2 -Limit $mineEnd -Description "D1 segment $segment side $side overlay"
                        $raw2 = $reader.ReadUInt16()
                    }
                    Assert-XfingSpan -Offset $stream.Position -Length 24 -Limit $mineEnd -Description "D1 segment $segment side $side UVL data"
                    $uvls = [System.Collections.Generic.List[object]]::new(4)
                    for ($uvlIndex = 0; $uvlIndex -lt 4; $uvlIndex++) {
                        $uvls.Add([pscustomobject]@{
                                u = $reader.ReadInt16()
                                v = $reader.ReadInt16()
                                l = $reader.ReadUInt16()
                            })
                    }
                    $rows.Add([pscustomobject]@{
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
                        })
                }
            }
        }
        if ($stream.Position -ne $mineEnd) {
            throw "D1 compiled mine consumed $($stream.Position - $mineOffset) bytes, expected $($mineEnd - $mineOffset)"
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
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth $Depth) + "`n", [Text.UTF8Encoding]::new($false))
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
            $relative = (Get-CompatibleRelativePath -BasePath $SourceDir -TargetPath $file.FullName).Replace('\', '/')
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
