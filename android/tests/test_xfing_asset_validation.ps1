$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
. (Join-Path $repoRoot "game_data\mods\xfing\xfing_minimal_dxa_lib.ps1")

$tempRoot = Join-Path $repoRoot "temp\test_xfing_asset_validation"
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

function Write-TestByteFile {
    param([string]$Name, [byte[]]$Bytes)

    $path = Join-Path $tempRoot $Name
    [System.IO.File]::WriteAllBytes($path, $Bytes)
    return $path
}

function ConvertTo-TestBinary {
    param([scriptblock]$Write)

    $stream = [System.IO.MemoryStream]::new()
    $writer = [System.IO.BinaryWriter]::new($stream)
    try {
        & $Write $writer
        $writer.Flush()
        return $stream.ToArray()
    } finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

function Write-FixedAscii {
    param([System.IO.BinaryWriter]$Writer, [string]$Text, [int]$Length)

    $bytes = [byte[]]::new($Length)
    $encoded = [System.Text.Encoding]::ASCII.GetBytes($Text)
    [Array]::Copy($encoded, $bytes, [Math]::Min($encoded.Length, $bytes.Length))
    $Writer.Write($bytes)
}

function ConvertTo-D2PigFixture {
    param([byte[]]$Payload = [byte[]](1, 2), [byte]$Flags = 0, [byte]$Width = 2, [byte]$Height = 1, [int]$Offset = 0)

    $stream = [System.IO.MemoryStream]::new()
    $writer = [System.IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([int]1195987024)
        $writer.Write([int]2)
        $writer.Write([int]1)
        Write-FixedAscii -Writer $writer -Text "TEST" -Length 8
        $writer.Write([byte]0)
        $writer.Write($Width)
        $writer.Write($Height)
        $writer.Write([byte]0)
        $writer.Write($Flags)
        $writer.Write([byte]0)
        $writer.Write($Offset)
        $writer.Write($Payload)
        $writer.Flush()
        return $stream.ToArray()
    } finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

function Assert-Rejected {
    param([string]$Name, [scriptblock]$Action)

    try {
        & $Action
    } catch {
        return
    }
    throw "$Name was accepted"
}

$d2Path = Write-TestByteFile "valid-d2.pig" (ConvertTo-D2PigFixture)
$d2 = Read-XfingD2Pig $d2Path
if ($d2.BitmapCount -ne 1 -or $d2.Entries[0].Length -ne 2) {
    throw "Valid D2 PIG did not preserve its entry"
}

$rlePayload = ConvertTo-TestBinary {
    param($writer)
    $writer.Write([int]8)
    $writer.Write([byte]3)
    $writer.Write([byte]1)
    $writer.Write([byte]2)
    $writer.Write([byte]0xe0)
}
$rlePath = Write-TestByteFile "valid-rle.pig" (ConvertTo-D2PigFixture -Payload $rlePayload -Flags 8)
$rle = Read-XfingD2Pig $rlePath
$pixels = Expand-XfingBitmapPixels -Pig $rle -Entry $rle.Entries[0]
if ($pixels.Count -ne 2 -or $pixels[0] -ne 1 -or $pixels[1] -ne 2) {
    throw "Valid bounded RLE did not decode exactly"
}

$d1Bytes = ConvertTo-TestBinary {
    param($writer)
    $writer.Write([int]4)
    $writer.Write([int]1)
    $writer.Write([int]0)
    Write-FixedAscii -Writer $writer -Text "D1TEST" -Length 8
    $writer.Write([byte]0)
    $writer.Write([byte]2)
    $writer.Write([byte]1)
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([int]0)
    $writer.Write([byte[]](3, 4))
}
$d1 = Read-XfingD1Pig (Write-TestByteFile "valid-d1.pig" $d1Bytes)
if ($d1.BitmapCount -ne 1 -or $d1.Entries[0].Length -ne 2) {
    throw "Valid D1 PIG did not preserve its entry"
}

$hamBytes = ConvertTo-TestBinary {
    param($writer)
    $writer.Write([int]558711112)
    $writer.Write([int]3)
    foreach ($count in 1..5) {
        $writer.Write([int]0)
    }
}
$ham = Read-XfingHamSections (Write-TestByteFile "valid.ham" $hamBytes)
if ($ham.NextOffset -ne 28) {
    throw "Valid minimal HAM ended at $($ham.NextOffset)"
}

$hogBytes = ConvertTo-TestBinary {
    param($writer)
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("DHF"))
    Write-FixedAscii -Writer $writer -Text "palette.256" -Length 13
    $writer.Write([int]3)
    $writer.Write([byte[]](1, 2, 3))
}
$hogPath = Write-TestByteFile "valid.hog" $hogBytes
$hogOutput = Join-Path $tempRoot "hog-output.bin"
Export-XfingHogEntry -HogPath $hogPath -EntryName "palette.256" -OutputPath $hogOutput
if ((Get-Item -LiteralPath $hogOutput).Length -ne 3) {
    throw "Valid HOG member was not exported exactly"
}

$levelBytes = ConvertTo-TestBinary {
    param($writer)
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("LVLP"))
    $writer.Write([int]1)
    $writer.Write([int]20)
    $writer.Write([int]25)
    $writer.Write([int]25)
    $writer.Write([byte]0)
    $writer.Write([int16]0)
    $writer.Write([int16]0)
}
$level = Read-XfingD1LevelSurfaces (Write-TestByteFile "valid.rdl" $levelBytes)
if ($level.vertexCount -ne 0 -or $level.segmentCount -ne 0) {
    throw "Valid minimal D1 level counts changed"
}

$negativeCount = [byte[]](ConvertTo-D2PigFixture).Clone()
[Array]::Copy([BitConverter]::GetBytes([int]-1), 0, $negativeCount, 8, 4)
Assert-Rejected "negative D2 count" { Read-XfingD2Pig (Write-TestByteFile "negative-count.pig" $negativeCount) }
Assert-Rejected "truncated D2 table" { Read-XfingD2Pig (Write-TestByteFile "truncated-table.pig" ([byte[]](ConvertTo-D2PigFixture)[0..20])) }
Assert-Rejected "zero D2 width" { Read-XfingD2Pig (Write-TestByteFile "zero-width.pig" (ConvertTo-D2PigFixture -Width 0)) }
Assert-Rejected "out-of-file D2 offset" { Read-XfingD2Pig (Write-TestByteFile "bad-offset.pig" (ConvertTo-D2PigFixture -Offset 20)) }
Assert-Rejected "short raw D2 payload" { Read-XfingD2Pig (Write-TestByteFile "short-raw.pig" (ConvertTo-D2PigFixture -Payload ([byte[]](1)))) }

$badRleLength = [byte[]]$rlePayload.Clone()
$badRleLength[4] = 2
Assert-Rejected "truncated RLE row" { Read-XfingD2Pig (Write-TestByteFile "short-rle-row.pig" (ConvertTo-D2PigFixture -Payload $badRleLength -Flags 8)) }
$badRleRun = ConvertTo-TestBinary {
    param($writer)
    $writer.Write([int]8)
    $writer.Write([byte]3)
    $writer.Write([byte]0xe3)
    $writer.Write([byte]7)
    $writer.Write([byte]0xe0)
}
Assert-Rejected "overlong RLE run" { Read-XfingD2Pig (Write-TestByteFile "overlong-rle.pig" (ConvertTo-D2PigFixture -Payload $badRleRun -Flags 8)) }
$trailingRle = [byte[]]$rlePayload.Clone()
$trailingRle[4] = 4
$trailingRle[0] = 9
$trailingRle += [byte]9
Assert-Rejected "trailing RLE bytes" { Read-XfingD2Pig (Write-TestByteFile "trailing-rle.pig" (ConvertTo-D2PigFixture -Payload $trailingRle -Flags 8)) }

$badD1Offset = [byte[]]$d1Bytes.Clone()
[Array]::Copy([BitConverter]::GetBytes([int]-1), 0, $badD1Offset, 25, 4)
Assert-Rejected "negative D1 entry offset" { Read-XfingD1Pig (Write-TestByteFile "negative-offset-d1.pig" $badD1Offset) }

$badHamCount = [byte[]]$hamBytes.Clone()
[Array]::Copy([BitConverter]::GetBytes([int]1201), 0, $badHamCount, 8, 4)
Assert-Rejected "extreme HAM texture count" { Read-XfingHamSections (Write-TestByteFile "extreme.ham" $badHamCount) }
Assert-Rejected "truncated HAM" { Read-XfingHamSections (Write-TestByteFile "truncated.ham" ([byte[]]$hamBytes[0..22])) }

$badHog = ConvertTo-TestBinary {
    param($writer)
    $writer.Write([System.Text.Encoding]::ASCII.GetBytes("DHF"))
    Write-FixedAscii -Writer $writer -Text "palette.256" -Length 13
    $writer.Write([int]4)
    $writer.Write([byte[]](1, 2))
}
[System.IO.File]::WriteAllBytes($hogOutput, [byte[]](9, 9, 9))
Assert-Rejected "truncated selected HOG member" { Export-XfingHogEntry -HogPath (Write-TestByteFile "truncated.hog" $badHog) -EntryName "palette.256" -OutputPath $hogOutput }
if ([BitConverter]::ToString([System.IO.File]::ReadAllBytes($hogOutput)) -ne "09-09-09") {
    throw "Rejected HOG replaced the prior output"
}

$badLevelSignature = [byte[]]$levelBytes.Clone()
$badLevelSignature[0] = [byte][char]'X'
Assert-Rejected "wrong level signature" { Read-XfingD1LevelSurfaces (Write-TestByteFile "bad-signature.rdl" $badLevelSignature) }
$badLevelCount = [byte[]]$levelBytes.Clone()
[Array]::Copy([BitConverter]::GetBytes([int16]-1), 0, $badLevelCount, 23, 2)
Assert-Rejected "negative level segment count" { Read-XfingD1LevelSurfaces (Write-TestByteFile "negative-segments.rdl" $badLevelCount) }
$extraMineByte = [byte[]]$levelBytes.Clone()
[Array]::Copy([BitConverter]::GetBytes([int]26), 0, $extraMineByte, 12, 4)
$extraMineByte += [byte]0
Assert-Rejected "extra compiled mine byte" { Read-XfingD1LevelSurfaces (Write-TestByteFile "extra-mine.rdl" $extraMineByte) }

Write-Output "XFing asset validation tests passed"
