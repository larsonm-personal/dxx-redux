$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "convert_d2xxl_sounds.ps1")

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("dxx-wav-test-" + [guid]::NewGuid().ToString("N"))
[System.IO.Directory]::CreateDirectory($tempRoot) | Out-Null

function Write-TestWav {
    param(
        [string]$Path,
        [byte[]]$Fmt,
        [byte[]]$Data,
        [uint32]$DataSize = $Data.Length
    )

    $stream = [System.IO.File]::Create($Path)
    try {
        $writer = New-Object System.IO.BinaryWriter($stream)
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes("RIFF"))
        $riffSize = 4 + 8 + $Fmt.Length + ($Fmt.Length % 2) + 8 + $Data.Length + ($Data.Length % 2)
        $writer.Write([uint32]$riffSize)
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes("WAVEfmt "))
        $writer.Write([uint32]$Fmt.Length)
        $writer.Write($Fmt)
        if ($Fmt.Length % 2) { $writer.Write([byte]0) }
        $writer.Write([System.Text.Encoding]::ASCII.GetBytes("data"))
        $writer.Write($DataSize)
        $writer.Write($Data)
        if ($Data.Length % 2) { $writer.Write([byte]0) }
    } finally {
        $stream.Dispose()
    }
}

function Expect-Rejected {
    param([string]$Path)

    try {
        Read-WavData $Path | Out-Null
        throw "Expected WAV rejection: $Path"
    } catch {
        if ($_.Exception.Message -like "Expected WAV rejection:*") { throw }
    }
}

try {
    $fmt = New-Object byte[] 16
    [BitConverter]::GetBytes([uint16]1).CopyTo($fmt, 0)
    [BitConverter]::GetBytes([uint16]1).CopyTo($fmt, 2)
    [BitConverter]::GetBytes([int32]11025).CopyTo($fmt, 4)
    [BitConverter]::GetBytes([int32]11025).CopyTo($fmt, 8)
    [BitConverter]::GetBytes([uint16]1).CopyTo($fmt, 12)
    [BitConverter]::GetBytes([uint16]8).CopyTo($fmt, 14)

    $valid = Join-Path $tempRoot "valid.wav"
    Write-TestWav $valid $fmt ([byte[]](0, 128, 255))
    $parsed = Read-WavData $valid
    if ($parsed.SampleRate -ne 11025 -or $parsed.Data.Length -ne 3) { throw "Valid WAV changed" }

    $shortFmt = Join-Path $tempRoot "short-fmt.wav"
    Write-TestWav $shortFmt ([byte[]](0, 0)) ([byte[]](0))
    Expect-Rejected $shortFmt

    $oversizedData = Join-Path $tempRoot "oversized-data.wav"
    Write-TestWav $oversizedData $fmt ([byte[]](0)) ([uint32]::MaxValue - 7)
    Expect-Rejected $oversizedData

    $misaligned = Join-Path $tempRoot "misaligned.wav"
    $fmt16Stereo = [byte[]]$fmt.Clone()
    [BitConverter]::GetBytes([uint16]2).CopyTo($fmt16Stereo, 2)
    [BitConverter]::GetBytes([uint16]4).CopyTo($fmt16Stereo, 12)
    [BitConverter]::GetBytes([uint16]16).CopyTo($fmt16Stereo, 14)
    Write-TestWav $misaligned $fmt16Stereo ([byte[]](0, 1, 2))
    Expect-Rejected $misaligned

    Write-Host "WAV parser tests passed"
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force
}
